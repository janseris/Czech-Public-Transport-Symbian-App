using System.Globalization;
using Pubtran.Api.Frpc;

namespace Pubtran.Api;

/// <summary>A place returned by suggest (city, stop, address, POI...). Serializable for "recent places".</summary>
public sealed record Place
{
    public string Name { get; init; } = "";          // suggestFirstRow
    public string Description { get; init; } = "";   // suggestSecondRow
    public string Category { get; init; } = "";      // municipality_cz, pubtran_cz, street_cz, poi_cz, ...
    public string Source { get; init; } = "";        // muni, pubt, stre, addr, poi, ... (sent back to getroutesopt)
    public long? Id { get; init; }
    public double? Lat { get; init; }
    public double? Lon { get; init; }
    public string? Municipality { get; init; }
    public string? District { get; init; }
    public string? Region { get; init; }

    public bool IsStop => Source == "pubt";
    public bool IsMunicipality => Source == "muni";

    /// <summary>Place as expected by getroutesopt searchOpts.start / end / mid.</summary>
    public FrpcStruct ToFrpc()
    {
        var s = new FrpcStruct();
        if (Lon is { } lon && Lat is { } lat) { s["x"] = lon; s["y"] = lat; }
        if (!string.IsNullOrEmpty(Source)) s["source"] = Source;
        if (Id is { } id) s["id"] = id;
        return s;
    }

    public override string ToString() => Name;

    internal static Place FromSuggest(FrpcStruct item)
    {
        var u = item.GetStruct("userData") ?? new FrpcStruct();
        double lat = u.GetDouble("latitude"), lon = u.GetDouble("longitude");
        return new Place
        {
            Name = u.GetString("suggestFirstRow", "")!,
            Description = u.GetString("suggestSecondRow", "")!,
            Category = item.GetString("category", "")!,
            Source = u.GetString("source", "")!,
            Id = u.GetLongOrNull("id"),
            Lat = double.IsNaN(lat) ? null : lat,
            Lon = double.IsNaN(lon) ? null : lon,
            Municipality = u.GetString("municipality"),
            District = u.GetString("district"),
            Region = u.GetString("region"),
        };
    }
}

/// <summary>vehicleType values, taken from cz.fhejl.pubtran.domain.TransportType.find().</summary>
public enum TransportType
{
    Unknown = 0, Bus = 1, Tram = 2, Ropeway = 3, Metro = 4, Boat = 5, Trolley = 6, Train = 7,
    Walk = -1, Wait = -2
}

public static class TransportTypeExt
{
    public static TransportType FromRaw(int raw) =>
        Enum.IsDefined(typeof(TransportType), raw) ? (TransportType)raw : TransportType.Unknown;

    public static string CzechName(this TransportType t) => t switch
    {
        TransportType.Bus => "Bus",
        TransportType.Tram => "Tram",
        TransportType.Ropeway => "Lanovka",
        TransportType.Metro => "Metro",
        TransportType.Boat => "Loď",
        TransportType.Trolley => "Trolejbus",
        TransportType.Train => "Vlak",
        TransportType.Walk => "Pěšky",
        TransportType.Wait => "Čekání",
        _ => "Spoj"
    };
}

/// <summary>
/// Numeric ids used in "info" arrays (per ride) and "stCodes" (per stop).
/// Names come from constants in the app (JourneyPart.INFO_*, RouteItem.FLAG_*), the rest inferred from traffic.
/// </summary>
public static class InfoCodes
{
    // ride info
    public const int MandatoryReservation = 14;  // text e.g. "Povinně místenkový spoj." (also a warning)
    public const int Accessible = 15;
    public const int Bicycle = 20;
    public const int Platform = 42;               // departure platform / stand (text)
    public const int NeedsBooking = 43;
    public const int Wifi = 45;
    public const int AirConditioning = 46;
    public const int Wagons = 123;
    public const int WagonsAccessible = 124;
    public const int NoDelay = 300;               // realtime: "aktuálně bez zpoždění", delay in seconds (can be negative)
    public const int Delay = 302;                 // realtime: delay seconds + text
    public const int VehiclePosition = 303;       // lat/lon of the vehicle
    public const int VehiclePosition2 = 305;
    public const int StopPlatform = 400;          // stopID + "arr / dep" track
    public const int UsualDelay = 500;            // statistical delay at stopID (seconds), text "p: probability, d: delay"

    public static readonly HashSet<int> Warnings = new() { 14, 101, 102, 103, 104, 105, 106, 107, 108, 201, 202, 203, 204, 205 };

    // stop codes (stCodes / startSCodes / endSCodes)
    public const int StopZone = 7;                // tariff zone text
    public const int StopAccessible = 24;
    public const int StopRequest = 28;            // request stop (na znamení)
    public const int StopOnlyGetOff = 37;
    public const int StopOnlyGetOn = 38;
    public const int StopNeedsBooking = 44;
    public const int StopEntranceExit = 443;      // Prague metro entrance/exit hint
}

public sealed class InfoItem
{
    public int Id { get; init; }
    public string Text { get; init; } = "";
    public string? Url { get; init; }
    public int? DelaySeconds { get; init; }
    public long? StopId { get; init; }
    public DateTimeOffset? Timestamp { get; init; }
    public double? Lat { get; init; }
    public double? Lon { get; init; }

    public bool IsWarning => InfoCodes.Warnings.Contains(Id);

    internal static InfoItem From(FrpcStruct s)
    {
        DateTimeOffset? ts = null;
        if (s.GetString("timestamp") is { } t && DateTimeOffset.TryParse(t, CultureInfo.InvariantCulture, DateTimeStyles.None, out var p)) ts = p;
        double lat = s.GetDouble("lat"), lon = s.GetDouble("lon");
        return new InfoItem
        {
            Id = s.GetInt("id"),
            Text = s.GetString("text", "")!,
            Url = s.GetString("url"),
            DelaySeconds = s.ContainsKey("delay") ? s.GetInt("delay") : null,
            StopId = s.GetLongOrNull("stopID"),
            Timestamp = ts,
            Lat = double.IsNaN(lat) ? null : lat,
            Lon = double.IsNaN(lon) ? null : lon,
        };
    }
}

public sealed class Stop
{
    public long Id { get; init; }
    public string Name { get; init; } = "";
    public double Lat { get; init; }
    public double Lon { get; init; }
    public long? ParentId { get; init; }

    internal static Stop From(FrpcStruct s)
    {
        var c = s.GetArray("coord");
        return new Stop
        {
            Id = s.GetLong("id"),
            Name = s.GetString("name", "")!,
            Lon = c.Count > 0 && c[0] is double x ? x : 0,
            Lat = c.Count > 1 && c[1] is double y ? y : 0,
            ParentId = s.GetLongOrNull("parent_id"),
        };
    }
}

/// <summary>One stop of a trip (with times).</summary>
public sealed class TripStop
{
    public Stop Stop { get; init; } = new();
    public DateTimeOffset Arrival { get; init; }
    public DateTimeOffset Departure { get; init; }
    public bool In { get; init; }
    public Dictionary<int, string> Codes { get; init; } = new();

    public string? Platform => Codes.TryGetValue(InfoCodes.StopPlatform, out var p) ? p
        : Codes.TryGetValue(InfoCodes.Platform, out var q) ? q : null;
    public string? Zone => Codes.TryGetValue(InfoCodes.StopZone, out var z) ? z : null;
    public bool IsRequestStop => Codes.ContainsKey(InfoCodes.StopRequest);
    public bool OnlyGetOff => Codes.ContainsKey(InfoCodes.StopOnlyGetOff);
    public bool OnlyGetOn => Codes.ContainsKey(InfoCodes.StopOnlyGetOn);
    public bool Accessible => Codes.ContainsKey(InfoCodes.StopAccessible);

    internal static TripStop FromTripData(FrpcStruct td, IReadOnlyList<Stop> stops)
    {
        int idx = td.GetInt("stopIndex", -1);
        var codes = new Dictionary<int, string>();
        foreach (var c in td.GetStructs("stCodes")) codes[c.GetInt("id")] = c.GetString("text", "")!;
        return new TripStop
        {
            Stop = idx >= 0 && idx < stops.Count ? stops[idx] : new Stop { Name = "?" },
            Arrival = IsoTime.Parse(td.GetString("arrivalISO")),
            Departure = IsoTime.Parse(td.GetString("departureISO")),
            In = td.GetBool("in"),
            Codes = codes,
        };
    }
}

public abstract class ConnectionPart
{
    public DateTimeOffset Departure { get; init; }
    public DateTimeOffset Arrival { get; init; }
    public abstract TransportType Type { get; }
}

public sealed class WalkPart : ConnectionPart
{
    public int DistanceMeters { get; init; }
    public (double Lon, double Lat) From { get; init; }
    public (double Lon, double Lat) To { get; init; }
    public override TransportType Type => TransportType.Walk;
}

public sealed class WaitPart : ConnectionPart
{
    public override TransportType Type => TransportType.Wait;
}

/// <summary>Riding one vehicle (route item type 0, or a trip returned by getnextdepartures).</summary>
public sealed class RidePart : ConnectionPart
{
    /// <summary>"line;;startId;;depISO;;endId;;arrISO;;startIdx;;endIdx" – used as tripId everywhere.</summary>
    public string PartDescription { get; init; } = "";
    public string RouteName { get; init; } = "";
    public string RouteLongName { get; init; } = "";
    public long? RoutePersistentId { get; init; }
    public long TripNumericId { get; init; }
    public int VehicleTypeRaw { get; init; }
    public override TransportType Type => TransportTypeExt.FromRaw(VehicleTypeRaw);
    public int Direction { get; init; }
    public string? StartPlatform { get; init; }
    public string? EndPlatform { get; init; }
    public int StartStopTripIndex { get; init; }
    public int EndStopTripIndex { get; init; }
    public List<TripStop> Stops { get; init; } = new();   // the WHOLE trip
    public List<InfoItem> Info { get; set; } = new();      // static + realtime info from the search
    public List<InfoItem>? LiveInfo { get; set; }          // refreshed by gettripinfos
    public string Agency { get; init; } = "";
    public string? AgencyUrl { get; init; }

    public TripStop? Boarding => StartStopTripIndex < Stops.Count ? Stops[StartStopTripIndex] : null;
    public TripStop? Alighting => EndStopTripIndex < Stops.Count ? Stops[EndStopTripIndex] : null;
    public IEnumerable<TripStop> RiddenStops =>
        Stops.Skip(StartStopTripIndex).Take(EndStopTripIndex - StartStopTripIndex + 1);

    private IEnumerable<InfoItem> CurrentInfo => LiveInfo is { Count: > 0 } ? LiveInfo.Concat(Info) : Info;

    /// <summary>Current delay in seconds (info 302 / 300), null when no realtime data.</summary>
    public int? DelaySeconds =>
        CurrentInfo.FirstOrDefault(i => i.Id is InfoCodes.Delay or InfoCodes.NoDelay)?.DelaySeconds;

    /// <summary>Statistical (usual) delay at a stop – info 500.</summary>
    public int? UsualDelayAt(long stopId) =>
        CurrentInfo.FirstOrDefault(i => i.Id == InfoCodes.UsualDelay && i.StopId == stopId)?.DelaySeconds;

    public string? PlatformAt(long stopId) =>
        CurrentInfo.FirstOrDefault(i => i.Id == InfoCodes.StopPlatform && i.StopId == stopId)?.Text.Trim();

    public IEnumerable<InfoItem> Warnings =>
        CurrentInfo.Where(i => i.IsWarning || i.Id == InfoCodes.Delay || i.Id == InfoCodes.NoDelay)
                   .GroupBy(i => (i.Id, i.Text)).Select(g => g.First());

    public bool Has(int infoId) => Info.Any(i => i.Id == infoId);

    public string Title => Type == TransportType.Train || RouteLongName.Length > RouteName.Length
        ? $"{Type.CzechName()} {RouteLongName}"
        : $"{Type.CzechName()} {RouteName}";

    internal static RidePart From(FrpcStruct item, IReadOnlyList<Stop> stops, IReadOnlyList<FrpcStruct> agencies)
    {
        var tripStops = item.GetStructs("tripData").Select(td => TripStop.FromTripData(td, stops)).ToList();
        int ag = item.GetInt("agencyIndex", -1);
        var agency = ag >= 0 && ag < agencies.Count ? agencies[ag] : null;
        return new RidePart
        {
            Departure = IsoTime.Parse(item.GetString("departureISO")),
            Arrival = IsoTime.Parse(item.GetString("arrivalISO")),
            PartDescription = item.GetString("partDescription", "")!,
            RouteName = item.GetString("routeName", "")!,
            RouteLongName = item.GetString("routeLongName", "")!,
            RoutePersistentId = item.GetLongOrNull("routePersistentID"),
            TripNumericId = item.GetLong("tripID"),
            VehicleTypeRaw = item.GetInt("vehicleType"),
            Direction = item.GetInt("direction"),
            StartPlatform = item.GetString("startPlatform"),
            EndPlatform = item.GetString("endPlatform"),
            StartStopTripIndex = item.GetInt("startStopTripIndex"),
            EndStopTripIndex = item.GetInt("endStopTripIndex"),
            Stops = tripStops,
            Info = item.GetStructs("info").Select(InfoItem.From).ToList(),
            Agency = agency?.GetString("short_name") is { Length: > 0 } sn ? sn : agency?.GetString("name", "") ?? "",
            AgencyUrl = agency?.GetString("url") is { Length: > 0 } u ? u : null,
        };
    }
}

public sealed class Payment
{
    public string Type { get; init; } = "";       // price | partialprice
    public double? Price { get; init; }
    public string? Currency { get; init; }
    public string? EshopName { get; init; }
    public string? Url { get; init; }
    public string? StopFrom { get; init; }
    public string? StopTo { get; init; }
    public string? SourceName { get; init; }

    internal static Payment From(FrpcStruct p)
    {
        double price = p.GetDouble("price");
        return new Payment
        {
            Type = p.GetString("type", "")!,
            Price = double.IsNaN(price) ? null : price,
            Currency = p.GetString("currency_type"),
            EshopName = p.GetString("eshopName"),
            Url = p.GetString("url"),
            StopFrom = p.GetString("stopFrom"),
            StopTo = p.GetString("stopTo"),
            SourceName = p.GetStruct("source")?.GetString("name"),
        };
    }
}

public sealed class Connection
{
    public long Hash { get; init; }
    public DateTimeOffset Departure { get; init; }
    public DateTimeOffset Arrival { get; init; }
    public int DurationSeconds { get; init; }
    public int TransferCount { get; init; }
    public List<ConnectionPart> Parts { get; init; } = new();
    public List<Payment> Payments { get; init; } = new();
    public Stop? StartStop { get; init; }
    public Stop? EndStop { get; init; }

    public IEnumerable<RidePart> Rides => Parts.OfType<RidePart>();
}

public sealed class SearchOptions
{
    public Place From { get; set; } = new();
    public Place To { get; set; } = new();
    public Place? Via { get; set; }
    public DateTime When { get; set; } = DateTime.Now;   // local (Europe/Prague) wall-clock time
    public bool IsDeparture { get; set; } = true;
    public int Count { get; set; } = 5;                  // SearchOptions.DOWNLOAD_LIMIT in the app

    public bool Train { get; set; } = true;
    public bool Bus { get; set; } = true;
    public bool Tram { get; set; } = true;
    public bool Trolley { get; set; } = true;
    public bool Metro { get; set; } = true;
    public bool Cable { get; set; } = true;
    public bool Ferry { get; set; } = true;

    public bool OnlyDirect { get; set; }
    public bool LowFloor { get; set; }
    public bool Bike { get; set; }
    public bool Stroller { get; set; }
    public string Language { get; set; } = "cs";
}

public sealed class TripDetail
{
    public int Direction { get; init; }
    public string EncodedGeometry { get; init; } = "";   // Seznam-encoded polyline (not decoded here)
    public List<(Stop Stop, int DistanceMeters, int TypeId)> Stops { get; init; } = new();
}
