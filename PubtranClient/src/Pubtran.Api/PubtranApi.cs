using System.Globalization;
using System.Net;
using System.Net.Http.Headers;
using Pubtran.Api.Frpc;

namespace Pubtran.Api;

public static class IsoTime
{
    public static DateTimeOffset Parse(string? s) =>
        string.IsNullOrEmpty(s) ? default : DateTimeOffset.Parse(s, CultureInfo.InvariantCulture);

    /// <summary>Backend wall-clock format without offset, e.g. 2026-09-30T20:56:00.</summary>
    public static string ToBackend(DateTime local) => local.ToString("yyyy-MM-dd'T'HH:mm:ss", CultureInfo.InvariantCulture);
    public static string ToBackend(DateTimeOffset t) => ToBackend(t.DateTime);
}

public sealed class RouteSearchResult
{
    public List<Connection> Connections { get; init; } = new();
    public int Status { get; init; }
}

/// <summary>
/// Client for https://pubtran-backend.mapy.cz/api/v1/ (Jízdní řády / cz.fhejl.pubtran).
/// Every call: POST {base}{method}, body = FastRPC 2.1 encoded struct of parameters,
/// headers Content-Type/Accept: application/x-frpc-rest. No auth, no API key.
/// </summary>
public sealed class PubtranApi : IDisposable
{
    public const string DefaultBaseUrl = "https://pubtran-backend.mapy.cz/api/v1/";
    private const string ContentType = "application/x-frpc-rest";
    private readonly HttpClient _http;

    public string Language { get; set; } = "cs";

    public PubtranApi(HttpMessageHandler? handler = null, string baseUrl = DefaultBaseUrl)
    {
        handler ??= new HttpClientHandler
        {
            AutomaticDecompression = DecompressionMethods.GZip,
            UseCookies = true,              // server sets a load-balancer cookie "sznlbr"
            CookieContainer = new CookieContainer(),
        };
        _http = new HttpClient(handler) { BaseAddress = new Uri(baseUrl), Timeout = TimeSpan.FromSeconds(20) };
        _http.DefaultRequestHeaders.UserAgent.ParseAdd("okhttp/5.4.0");
        _http.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue(ContentType));
    }

    public void Dispose() => _http.Dispose();

    /// <summary>Low-level call: encodes params, POSTs, decodes the response struct.</summary>
    public async Task<FrpcStruct> CallAsync(string method, FrpcStruct parameters, CancellationToken ct = default)
    {
        using var content = new ByteArrayContent(FrpcCodec.Encode(parameters));
        content.Headers.ContentType = new MediaTypeHeaderValue(ContentType);
        using var resp = await _http.PostAsync(method, content, ct).ConfigureAwait(false);
        var body = await resp.Content.ReadAsByteArrayAsync(ct).ConfigureAwait(false);
        if (!resp.IsSuccessStatusCode)
            throw new HttpRequestException($"{method}: HTTP {(int)resp.StatusCode} {resp.ReasonPhrase}");
        return FrpcCodec.Decode(body) as FrpcStruct
               ?? throw new FormatException($"{method}: response is not a struct");
    }

    // ------------------------------------------------------------------ suggest
    /// <summary>
    /// Place autocomplete. lat/lon = position used to rank results (the app sends the phone's location).
    /// </summary>
    public async Task<List<Place>> SuggestAsync(string query, double? lat = null, double? lon = null,
        int count = 10, CancellationToken ct = default)
    {
        var p = new FrpcStruct
        {
            { "query", query },
            { "count", count },
        };
        if (lon is { } x) p["lon"] = x;
        p["category"] = "municipality|street|address|poi|area|firm|pubt";
        p["lang"] = Language;
        if (lat is { } y) p["lat"] = y;

        var r = await CallAsync("suggest", p, ct).ConfigureAwait(false);
        return r.GetStructs("result").Select(Place.FromSuggest).ToList();
    }

    // ------------------------------------------------------------ getroutesopt
    /// <summary>
    /// Connection search. Paging: same "when", index 0 = first page, +Count = later, -Count = earlier;
    /// hashesUsed = hashes of connections already shown (server skips them).
    /// </summary>
    public async Task<RouteSearchResult> SearchAsync(SearchOptions o, int index = 0,
        IEnumerable<long>? hashesUsed = null, CancellationToken ct = default)
    {
        var flags = new FrpcStruct
        {
            { "bus", o.Bus ? 1 : 0 },
            { "unixt", 1 },
            { "mobile", 1 },
            { "lowfloor", o.LowFloor ? 1 : 0 },
            { "version", 3 },
            { "bike", o.Bike ? 1 : 0 },
            { "trolley", o.Trolley ? 1 : 0 },
            { "metro", o.Metro ? 1 : 0 },
            { "ferry", o.Ferry ? 1 : 0 },
            { "hashes_used", (hashesUsed ?? Enumerable.Empty<long>()).ToList() },
            { "geometry", 0 },
            { "stroller", o.Stroller ? 1 : 0 },
            { "tram", o.Tram ? 1 : 0 },
            { "cable", o.Cable ? 1 : 0 },
            { "train", o.Train ? 1 : 0 },
        };
        if (o.OnlyDirect) flags["tcount"] = 0;

        var searchOpts = new FrpcStruct
        {
            { "start", o.From.ToFrpc() },
            { "count", o.Count },
            { "index", index },
            { "end", o.To.ToFrpc() },
            { "lang", new List<object?> { o.Language } },
        };
        if (o.Via != null) searchOpts["mid"] = new List<object?> { o.Via.ToFrpc() };

        var p = new FrpcStruct
        {
            { "isDeparture", o.IsDeparture },
            { "flags", flags },
            { "searchOpts", searchOpts },
            { "when", IsoTime.ToBackend(o.When) },
        };

        var r = await CallAsync("getroutesopt", p, ct).ConfigureAwait(false);
        int status = r.GetInt("status", 200);
        var stops = r.GetStructs("stops").Select(Stop.From).ToList();
        var agencies = r.GetStructs("agenciesinfo").ToList();

        var list = new List<Connection>();
        foreach (var route in r.GetStructs("routes"))
        {
            var parts = new List<ConnectionPart>();
            foreach (var it in route.GetStructs("items"))
            {
                switch (it.GetInt("type"))
                {
                    case 0:
                        parts.Add(RidePart.From(it, stops, agencies));
                        break;
                    case 1:
                        parts.Add(new WalkPart
                        {
                            Departure = IsoTime.Parse(it.GetString("departureISO")),
                            Arrival = IsoTime.Parse(it.GetString("arrivalISO")),
                            DistanceMeters = it.GetInt("distance"),
                            From = Coord(it.GetArray("oznStartCoord")),
                            To = Coord(it.GetArray("oznEndCoord")),
                        });
                        break;
                    default:
                        parts.Add(new WaitPart
                        {
                            Departure = IsoTime.Parse(it.GetString("departureISO")),
                            Arrival = IsoTime.Parse(it.GetString("arrivalISO")),
                        });
                        break;
                }
            }

            int si = route.GetInt("startStopIndex", -1), ei = route.GetInt("endStopIndex", -1);
            list.Add(new Connection
            {
                Hash = route.GetLong("hash"),
                Departure = IsoTime.Parse(route.GetString("departureISO")),
                Arrival = IsoTime.Parse(route.GetString("arrivalISO")),
                DurationSeconds = route.GetInt("time"),
                TransferCount = route.GetInt("transferCount"),
                Parts = parts,
                Payments = route.GetStructs("payment").Select(Payment.From).ToList(),
                StartStop = si >= 0 && si < stops.Count ? stops[si] : null,
                EndStop = ei >= 0 && ei < stops.Count ? stops[ei] : null,
            });
        }
        return new RouteSearchResult { Connections = list, Status = status };
    }

    private static (double, double) Coord(List<object?> a) =>
        (a.Count > 0 && a[0] is double x ? x : 0, a.Count > 1 && a[1] is double y ? y : 0);

    // ------------------------------------------------------- getnextdepartures
    /// <summary>
    /// Other runs of the same line between the same stops ("swipe" in the app).
    /// offset: -1 previous run, 1 next, 2 the one after... always relative to the ORIGINAL ride.
    /// Returns null for an entry the server could not resolve.
    /// </summary>
    public async Task<List<RidePart?>> GetOtherRunsAsync(IEnumerable<(RidePart ride, int offset)> requests,
        SearchOptions? modes = null, CancellationToken ct = default)
    {
        modes ??= new SearchOptions();
        var arr = new List<object?>();
        foreach (var (ride, offset) in requests)
        {
            var opts = new FrpcStruct
            {
                { "bus", modes.Bus ? 1 : 0 },
                { "unixt", 1 },
                { "lowfloor", modes.LowFloor ? 1 : 0 },
                { "trolley", modes.Trolley ? 1 : 0 },
                { "endindex", ride.EndStopTripIndex },
                { "startindex", ride.StartStopTripIndex },
                { "metro", modes.Metro ? 1 : 0 },
                { "ferry", modes.Ferry ? 1 : 0 },
                { "geometry", 0 },
                { "stops", 1 },
                { "tripdata", 1 },
                { "time", IsoTime.ToBackend(ride.Departure) },
                { "lang", new List<object?> { Language } },
                { "tram", modes.Tram ? 1 : 0 },
                { "cable", modes.Cable ? 1 : 0 },
                { "reqindex", offset },
                { "train", modes.Train ? 1 : 0 },
            };
            arr.Add(new List<object?> { ride.PartDescription, opts });
        }

        var r = await CallAsync("getnextdepartures", new FrpcStruct { { "params", arr } }, ct).ConfigureAwait(false);
        var result = new List<RidePart?>();
        foreach (var res in r.GetStructs("results"))
        {
            if (res.GetInt("status") != 200 || res.GetStruct("trip") is not { } trip) { result.Add(null); continue; }
            var stops = trip.GetStructs("stops").Select(Stop.From).ToList();
            var agencies = trip.GetStructs("agenciesinfo").ToList();
            result.Add(RidePart.From(trip, stops, agencies));
        }
        return result;
    }

    public async Task<RidePart?> GetOtherRunAsync(RidePart original, int offset, SearchOptions? modes = null,
        CancellationToken ct = default) =>
        (await GetOtherRunsAsync(new[] { (original, offset) }, modes, ct).ConfigureAwait(false)).FirstOrDefault();

    // ------------------------------------------------------------ gettripinfos
    /// <summary>Realtime info (delays, disruptions, platforms) for rides. One list per input ride.</summary>
    public async Task<List<List<InfoItem>>> GetTripInfosAsync(IEnumerable<RidePart> rides, CancellationToken ct = default)
    {
        var ids = rides.Select(r => (object?)new FrpcStruct
        {
            { "tripId", r.PartDescription },
            { "time", r.Departure },          // FastRPC datetime
        }).ToList();
        if (ids.Count == 0) return new();

        var r = await CallAsync("gettripinfos", new FrpcStruct { { "tripIds", ids } }, ct).ConfigureAwait(false);
        return r.GetStructs("results")
            .Select(x => x.GetStructs("info").Select(InfoItem.From).ToList())
            .ToList();
    }

    // ----------------------------------------------------------- gettripdetail
    /// <summary>Stops + geometry of a ride (the app uses it for the map).</summary>
    public async Task<TripDetail> GetTripDetailAsync(string tripId, CancellationToken ct = default)
    {
        var r = await CallAsync("gettripdetail",
            new FrpcStruct { { "tripId", tripId }, { "params", new FrpcStruct() } }, ct).ConfigureAwait(false);
        var trip = r.GetStruct("trip") ?? new FrpcStruct();
        return new TripDetail
        {
            Direction = trip.GetInt("direction"),
            EncodedGeometry = trip.GetString("geom", "")!,
            Stops = trip.GetStructs("stops")
                .Select(s => (Stop.From(s), s.GetInt("distance"), s.GetInt("typeId"))).ToList(),
        };
    }

    // ------------------------------------------------------------- getwalkgeom
    /// <summary>Walking path between two points: (distance in m, encoded geometry).</summary>
    public async Task<(int DistanceMeters, string EncodedGeometry)> GetWalkGeometryAsync(
        (double Lon, double Lat) from, (double Lon, double Lat) to, CancellationToken ct = default)
    {
        var p = new FrpcStruct
        {
            { "start", new FrpcStruct { { "x", from.Lon }, { "y", from.Lat } } },
            { "end", new FrpcStruct { { "x", to.Lon }, { "y", to.Lat } } },
        };
        var r = await CallAsync("getwalkgeom", p, ct).ConfigureAwait(false);
        var g = r.GetStruct("geom") ?? new FrpcStruct();
        return (g.GetInt("distance"), g.GetString("geom", "")!);
    }
}
