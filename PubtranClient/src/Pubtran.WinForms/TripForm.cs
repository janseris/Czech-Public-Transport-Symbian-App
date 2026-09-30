using Pubtran.Api;

namespace Pubtran.WinForms;

/// <summary>
/// Detail of one ride (all stops of the trip). "◀ / ▶" buttons replace the app's swipe:
/// they load other runs of the same line between the same stops via getnextdepartures,
/// with reqindex relative to the original ride (-1, +1, +2, ...).
/// </summary>
internal sealed class TripForm : Form
{
    private readonly PubtranApi _api;
    private readonly RidePart _original;
    private readonly SearchOptions? _modes;
    private readonly Dictionary<int, RidePart?> _cache = new();
    private int _offset;
    private readonly Font _bold;
    private RidePart? _current;

    private readonly Label _hdr = new() { Dock = DockStyle.Top, Height = 64, Padding = new Padding(8, 6, 8, 0), Font = new Font("Segoe UI", 10.5f) };
    private readonly Label _pos = new() { AutoSize = true, Padding = new Padding(8, 8, 8, 0), ForeColor = Color.DimGray };
    private readonly Button _prev = new() { Text = "◀ Předchozí spoj", AutoSize = true };
    private readonly Button _next = new() { Text = "Následující spoj ▶", AutoSize = true };
    private readonly Button _refresh = new() { Text = "Obnovit zpoždění", AutoSize = true };
    private readonly ListView _lv = new() { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, GridLines = false, HideSelection = false };
    private readonly TextBox _notes = new() { Dock = DockStyle.Bottom, Height = 110, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, BackColor = SystemColors.Window };
    private readonly Label _status = new() { Dock = DockStyle.Bottom, Height = 22, ForeColor = Color.DimGray, Padding = new Padding(6, 3, 0, 0) };

    public TripForm(PubtranApi api, RidePart original, int offset, SearchOptions? modes)
    {
        _api = api; _original = original; _offset = offset; _modes = modes;
        _cache[0] = original;

        Text = "Detail spoje – " + original.Title;
        Font = new Font("Segoe UI", 9.5f);
        _bold = new Font(Font, FontStyle.Bold);
        Size = new Size(760, 640);
        MinimumSize = new Size(520, 400);
        StartPosition = FormStartPosition.CenterParent;
        ShowInTaskbar = false;
        KeyPreview = true;

        var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 38, Padding = new Padding(4) };
        var close = new Button { Text = "Zpět", AutoSize = true, DialogResult = DialogResult.Cancel };
        bar.Controls.AddRange(new Control[] { _prev, _next, _refresh, close, _pos });
        CancelButton = close;

        _lv.Columns.Add("Příjezd", 90);
        _lv.Columns.Add("Odjezd", 90);
        _lv.Columns.Add("Zastávka", 260);
        _lv.Columns.Add("Nást.", 60);
        _lv.Columns.Add("Pásmo", 80);
        _lv.Columns.Add("Poznámka", 160);

        Controls.Add(_lv);
        Controls.Add(bar);
        Controls.Add(_hdr);
        Controls.Add(_notes);
        Controls.Add(_status);

        _prev.Click += async (_, _) => { _offset--; await LoadAsync(); };
        _next.Click += async (_, _) => { _offset++; await LoadAsync(); };
        _refresh.Click += async (_, _) => { if (_current != null) { await RefreshLiveAsync(_current); Render(); } };
        KeyDown += async (_, e) =>
        {
            if (e.KeyCode == Keys.Left && e.Control) { _offset--; await LoadAsync(); }
            else if (e.KeyCode == Keys.Right && e.Control) { _offset++; await LoadAsync(); }
        };
        Shown += async (_, _) => await LoadAsync();
    }

    private async Task LoadAsync()
    {
        SetBusy(true, "Načítám spoj…");
        try
        {
            if (!_cache.TryGetValue(_offset, out var run))
            {
                run = await _api.GetOtherRunAsync(_original, _offset, _modes);
                _cache[_offset] = run;
            }
            if (run == null)
            {
                _status.Text = "Spoj nenalezen.";
                _current = null;
                _lv.Items.Clear();
                _hdr.Text = "";
                return;
            }
            _current = run;
            Render();
            await RefreshLiveAsync(run);
            Render();
            _status.Text = "";
        }
        catch (Exception ex)
        {
            _status.Text = "Chyba: " + ex.Message;
        }
        finally { SetBusy(false, null); }
    }

    private async Task RefreshLiveAsync(RidePart run)
    {
        try
        {
            var infos = await _api.GetTripInfosAsync(new[] { run });
            if (infos.Count > 0) run.LiveInfo = infos[0];
        }
        catch (Exception ex) { _status.Text = "Zpoždění nelze načíst: " + ex.Message; }
    }

    private void SetBusy(bool busy, string? text)
    {
        _prev.Enabled = _next.Enabled = _refresh.Enabled = !busy;
        UseWaitCursor = busy;
        if (text != null) _status.Text = text;
    }

    private void Render()
    {
        var r = _current;
        if (r == null) return;

        _pos.Text = _offset switch
        {
            0 => "zvolený spoj",
            > 0 => $"{_offset}. následující",
            _ => $"{-_offset}. předchozí"
        };

        var delay = Fmt.Delay(r.DelaySeconds);
        _hdr.Text = $"{r.Title}   ·   {r.Agency}{(delay != null ? "   ·   " + delay : "")}\n" +
                    $"{r.Boarding?.Stop.Name} {Fmt.TimeWithDay(r.Departure, DateTime.Today)}  →  " +
                    $"{r.Alighting?.Stop.Name} {Fmt.TimeWithDay(r.Arrival, DateTime.Today)}   ({Fmt.Duration(r.Departure, r.Arrival)})";
        _hdr.ForeColor = r.DelaySeconds is > 60 ? Color.Firebrick : SystemColors.ControlText;

        _lv.BeginUpdate();
        _lv.Items.Clear();
        for (int i = 0; i < r.Stops.Count; i++)
        {
            var s = r.Stops[i];
            bool first = i == 0, last = i == r.Stops.Count - 1;
            bool ridden = i >= r.StartStopTripIndex && i <= r.EndStopTripIndex;
            bool endpoint = i == r.StartStopTripIndex || i == r.EndStopTripIndex;

            string platform = r.PlatformAt(s.Stop.Id) ?? s.Platform ?? "";
            if (i == r.StartStopTripIndex && string.IsNullOrEmpty(platform)) platform = r.StartPlatform ?? "";
            if (i == r.EndStopTripIndex && string.IsNullOrEmpty(platform)) platform = r.EndPlatform ?? "";

            var item = new ListViewItem(new[]
            {
                first ? "" : Fmt.Time(s.Arrival),
                last ? "" : Fmt.Time(s.Departure),
                s.Stop.Name,
                platform,
                s.Zone ?? "",
                Fmt.StopNotes(s, r),
            })
            {
                UseItemStyleForSubItems = true,
                ForeColor = ridden ? SystemColors.WindowText : Color.Gray,
            };
            if (endpoint) item.Font = _bold;
            if (ridden) item.BackColor = Color.FromArgb(240, 246, 255);
            _lv.Items.Add(item);
        }
        _lv.EndUpdate();
        if (r.StartStopTripIndex < _lv.Items.Count) _lv.EnsureVisible(r.StartStopTripIndex);

        var lines = new List<string>();
        lines.AddRange(Fmt.RideNotes(r));
        lines.Add($"  tripId: {r.PartDescription}");
        _notes.Text = string.Join(Environment.NewLine, lines);
    }
}
