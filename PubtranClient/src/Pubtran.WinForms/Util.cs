using System.Globalization;
using System.Text.Json;
using Pubtran.Api;

namespace Pubtran.WinForms;

internal static class Fmt
{
    private static readonly CultureInfo Cz = CultureInfo.GetCultureInfo("cs-CZ");

    public static string Time(DateTimeOffset t) => t == default ? "" : t.ToString("HH:mm", Cz);

    /// <summary>Time, plus the date when it is not the reference day.</summary>
    public static string TimeWithDay(DateTimeOffset t, DateTime referenceDay) =>
        t.Date == referenceDay.Date ? Time(t) : $"{Time(t)} ({t.ToString("ddd d.M.", Cz)})";

    public static string DayHeader(DateTimeOffset t) => t.ToString("dddd d. M.", Cz);

    public static string Duration(int seconds)
    {
        var ts = TimeSpan.FromSeconds(seconds);
        return ts.TotalHours >= 1 ? $"{(int)ts.TotalHours} h {ts.Minutes} min" : $"{Math.Max(0, (int)ts.TotalMinutes)} min";
    }

    public static string Duration(DateTimeOffset from, DateTimeOffset to) => Duration((int)(to - from).TotalSeconds);

    public static string Transfers(int n) => n switch
    {
        0 => "bez přestupu",
        1 => "1 přestup",
        >= 2 and <= 4 => $"{n} přestupy",
        _ => $"{n} přestupů"
    };

    /// <summary>null = no realtime info.</summary>
    public static string? Delay(int? seconds)
    {
        if (seconds is not { } s) return null;
        int min = (int)Math.Round(s / 60.0);
        return min <= 0 ? "včas" : $"zpoždění +{min} min";
    }

    public static string StopNotes(TripStop s, RidePart ride)
    {
        var parts = new List<string>();
        if (s.IsRequestStop) parts.Add("na znamení");
        if (s.OnlyGetOff) parts.Add("jen výstup");
        if (s.OnlyGetOn) parts.Add("jen nástup");
        if (s.Accessible) parts.Add("bezbariérová");
        if (ride.UsualDelayAt(s.Stop.Id) is { } ud && ud >= 60) parts.Add($"obvykle +{(int)Math.Round(ud / 60.0)} min");
        return string.Join(", ", parts);
    }

    public static string Amenities(RidePart r)
    {
        var a = new List<string>();
        if (r.Has(InfoCodes.Wifi)) a.Add("Wi-Fi");
        if (r.Has(InfoCodes.AirConditioning)) a.Add("klimatizace");
        if (r.Has(InfoCodes.Accessible)) a.Add("bezbariérový");
        if (r.Has(InfoCodes.Bicycle)) a.Add("přeprava kol");
        if (r.Has(InfoCodes.NeedsBooking)) a.Add("nutná rezervace");
        return string.Join(", ", a);
    }

    /// <summary>Warnings, amenities and realtime messages of a ride as plain text lines.</summary>
    public static IEnumerable<string> RideNotes(RidePart r)
    {
        var am = Amenities(r);
        if (am.Length > 0) yield return "  Vybavení: " + am;
        foreach (var w in r.Warnings)
        {
            var text = w.Id is InfoCodes.Delay or InfoCodes.NoDelay ? $"{Delay(w.DelaySeconds)} – {w.Text}" : w.Text;
            if (string.IsNullOrWhiteSpace(text)) continue;
            yield return "  ⚠ " + text.Trim() + (w.Url != null ? $"  ({w.Url})" : "");
        }
        if (r.AgencyUrl != null) yield return "  Dopravce: " + r.Agency + " – " + r.AgencyUrl;
    }

    public static string PlaceKind(Place p) => p.Source switch
    {
        "muni" => "obec",
        "pubt" => "zastávka",
        "stre" or "street" => "ulice",
        "addr" => "adresa",
        "ward" or "quar" => "část obce",
        "" => "",
        _ => "místo"
    };
}

/// <summary>Recently used places, stored in %APPDATA%\PubtranClient\recent.json (the app keeps them locally too).</summary>
internal static class RecentPlaces
{
    private static string FilePath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PubtranClient", "recent.json");

    public static List<Place> Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<List<Place>>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { /* corrupted file -> start over */ }
        return new();
    }

    public static void Add(Place p)
    {
        try
        {
            var list = Load();
            list.RemoveAll(x => x.Source == p.Source && x.Id == p.Id && x.Name == p.Name);
            list.Insert(0, p);
            if (list.Count > 15) list.RemoveRange(15, list.Count - 15);
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { /* not critical */ }
    }
}
