using Pubtran.Api;

namespace Pubtran.WinForms;

/// <summary>
/// Main window mirroring the app: Odkud / Kam / (Přes), time, Hledat, list of connections with
/// "earlier" / "later" paging, and a detail pane for the selected connection.
/// </summary>
internal sealed class MainForm : Form
{
    private readonly PubtranApi _api = new();

    // search state
    private Place? _from, _to, _via;
    private SearchOptions? _opts;
    private readonly List<Connection> _connections = new();
    private int _minIndex, _maxIndex;
    private Connection? _selected;
    private RidePart? _selectedRide;
    private CancellationTokenSource? _searchCts;

    // --- search panel
    private readonly TextBox _txtFrom = MakePlaceBox("Odkud?");
    private readonly TextBox _txtTo = MakePlaceBox("Kam?");
    private readonly TextBox _txtVia = MakePlaceBox("Přes (nepovinné)");
    private readonly Button _btnSwap = new() { Text = "⇅", Width = 36, Height = 28 };
    private readonly Button _btnClearVia = new() { Text = "✕", Width = 36, Height = 28 };
    private readonly DateTimePicker _dtpDate = new() { Format = DateTimePickerFormat.Short, Width = 110 };
    private readonly DateTimePicker _dtpTime = new() { Format = DateTimePickerFormat.Custom, CustomFormat = "HH:mm", ShowUpDown = true, Width = 70 };
    private readonly Button _btnNow = new() { Text = "Teď", AutoSize = true };
    private readonly RadioButton _rbDep = new() { Text = "Odjezd", Checked = true, AutoSize = true };
    private readonly RadioButton _rbArr = new() { Text = "Příjezd", AutoSize = true };
    private readonly CheckBox _chkDirect = new() { Text = "Jen přímé", AutoSize = true };
    private readonly CheckBox _chkTrain = Mode("Vlak"), _chkBus = Mode("Bus"), _chkTram = Mode("Tram"),
        _chkTrolley = Mode("Trolejbus"), _chkMetro = Mode("Metro"), _chkCable = Mode("Lanovka"), _chkFerry = Mode("Loď");
    private readonly CheckBox _chkLowFloor = new() { Text = "Nízkopodlažní", AutoSize = true };
    private readonly CheckBox _chkBike = new() { Text = "Kolo", AutoSize = true };
    private readonly CheckBox _chkStroller = new() { Text = "Kočárek", AutoSize = true };
    private readonly Button _btnSearch = new() { Text = "Hledat", Height = 34, Dock = DockStyle.Fill, Font = new Font("Segoe UI", 11f, FontStyle.Bold) };

    // --- results
    private readonly ListBox _lstConn = new() { Dock = DockStyle.Fill, DrawMode = DrawMode.OwnerDrawFixed, IntegralHeight = false, BorderStyle = BorderStyle.None };
    private readonly Button _btnEarlier = new() { Text = "▲ Předchozí spoje", Dock = DockStyle.Top, Height = 30, Enabled = false };
    private readonly Button _btnLater = new() { Text = "▼ Další spoje", Dock = DockStyle.Bottom, Height = 30, Enabled = false };

    // --- detail
    private readonly Label _lblHeader = new() { Dock = DockStyle.Top, Height = 48, Padding = new Padding(6, 6, 6, 0), Font = new Font("Segoe UI", 10.5f, FontStyle.Bold) };
    private readonly CheckBox _chkAllStops = new() { Text = "Všechny zastávky", AutoSize = true, Margin = new Padding(3, 7, 12, 3) };
    private readonly Button _btnPrevRun = new() { Text = "◀ Předchozí spoj", AutoSize = true, Enabled = false };
    private readonly Button _btnTrip = new() { Text = "Detail spoje", AutoSize = true, Enabled = false };
    private readonly Button _btnNextRun = new() { Text = "Následující spoj ▶", AutoSize = true, Enabled = false };
    private readonly Button _btnRefresh = new() { Text = "Obnovit zpoždění", AutoSize = true, Enabled = false };
    private readonly ListView _lvDetail = new() { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, HideSelection = false, ShowGroups = true };
    private readonly TextBox _txtNotes = new() { Dock = DockStyle.Bottom, Height = 120, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, BackColor = SystemColors.Window };

    private readonly ToolStripStatusLabel _status = new() { Spring = true, TextAlign = ContentAlignment.MiddleLeft };
    private readonly System.Windows.Forms.Timer _liveTimer = new() { Interval = 60_000 };
    private readonly Font _bold;

    public MainForm()
    {
        Text = "Jízdní řády – vlastní klient (pubtran-backend.mapy.cz)";
        Font = new Font("Segoe UI", 9.5f);
        _bold = new Font(Font, FontStyle.Bold);
        Size = new Size(1280, 820);
        MinimumSize = new Size(900, 600);
        StartPosition = FormStartPosition.CenterScreen;

        BuildLayout();
        WireEvents();
        SetNow();
        UpdatePlaceBoxes();
    }

    private static TextBox MakePlaceBox(string placeholder) => new()
    {
        ReadOnly = true, Cursor = Cursors.Hand, PlaceholderText = placeholder, BackColor = SystemColors.Window,
        Dock = DockStyle.Fill, Font = new Font("Segoe UI", 11f), Margin = new Padding(3, 2, 3, 2)
    };

    private static CheckBox Mode(string text) => new() { Text = text, Checked = true, AutoSize = true };

    // ============================================================== layout
    private void BuildLayout()
    {
        var search = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 3, Padding = new Padding(8, 8, 8, 4) };
        search.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 56));
        search.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        search.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 44));

        void Row(string label, Control box, Control? right)
        {
            int r = search.RowCount++;
            search.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            search.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 8, 0, 0) }, 0, r);
            search.Controls.Add(box, 1, r);
            if (right != null) search.Controls.Add(right, 2, r);
        }
        Row("Odkud", _txtFrom, _btnSwap);
        Row("Kam", _txtTo, null);
        Row("Přes", _txtVia, _btnClearVia);

        var timeRow = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, WrapContents = true, Margin = new Padding(0) };
        var sep = new Label { Width = 16 };
        timeRow.Controls.AddRange(new Control[] { _dtpDate, _dtpTime, _btnNow, sep, _rbDep, _rbArr, new Label { Width = 16 }, _chkDirect });
        Row("Kdy", timeRow, null);

        var modes = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, WrapContents = true, Margin = new Padding(0) };
        modes.Controls.AddRange(new Control[] { _chkTrain, _chkBus, _chkTram, _chkTrolley, _chkMetro, _chkCable, _chkFerry,
            new Label { Width = 16 }, _chkLowFloor, _chkBike, _chkStroller });
        Row("Druh", modes, null);

        int sr = search.RowCount++;
        search.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        search.Controls.Add(_btnSearch, 1, sr);

        // results (left) + detail (right)
        var split = new SplitContainer { Dock = DockStyle.Fill, FixedPanel = FixedPanel.Panel1 };
        Load += (_, _) => split.SplitterDistance = Math.Min(480, Math.Max(200, split.Width / 2 - 40));
        split.Panel1.Controls.Add(_lstConn);
        split.Panel1.Controls.Add(_btnEarlier);
        split.Panel1.Controls.Add(_btnLater);

        var legBar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 38, Padding = new Padding(2) };
        legBar.Controls.AddRange(new Control[] { _chkAllStops, _btnPrevRun, _btnTrip, _btnNextRun, _btnRefresh });

        _lvDetail.Columns.Add("Příjezd", 90);
        _lvDetail.Columns.Add("Odjezd", 90);
        _lvDetail.Columns.Add("Zastávka", 280);
        _lvDetail.Columns.Add("Nást.", 60);
        _lvDetail.Columns.Add("Poznámka", 220);

        split.Panel2.Controls.Add(_lvDetail);
        split.Panel2.Controls.Add(legBar);
        split.Panel2.Controls.Add(_lblHeader);
        split.Panel2.Controls.Add(_txtNotes);

        var statusStrip = new StatusStrip();
        statusStrip.Items.Add(_status);

        Controls.Add(split);
        Controls.Add(search);
        Controls.Add(statusStrip);

        _lstConn.ItemHeight = Math.Min(255, (int)(Font.Height * 4.4) + 6);
    }

    private void WireEvents()
    {
        _txtFrom.Click += (_, _) => PickPlace(ref _from, "Odkud");
        _txtTo.Click += (_, _) => PickPlace(ref _to, "Kam");
        _txtVia.Click += (_, _) => PickPlace(ref _via, "Přes");
        _btnSwap.Click += (_, _) => { (_from, _to) = (_to, _from); UpdatePlaceBoxes(); };
        _btnClearVia.Click += (_, _) => { _via = null; UpdatePlaceBoxes(); };
        _btnNow.Click += (_, _) => SetNow();
        _btnSearch.Click += async (_, _) => await SearchAsync();

        _btnLater.Click += async (_, _) => await LoadPageAsync(later: true);
        _btnEarlier.Click += async (_, _) => await LoadPageAsync(later: false);

        _lstConn.DrawItem += LstConn_DrawItem;
        _lstConn.SelectedIndexChanged += (_, _) => { _selected = _lstConn.SelectedItem as Connection; RenderDetail(); };
        _lstConn.DoubleClick += (_, _) => { _chkAllStops.Checked = !_chkAllStops.Checked; };  // "click again = more details"

        _chkAllStops.CheckedChanged += (_, _) => RenderDetail();
        _lvDetail.SelectedIndexChanged += (_, _) =>
        {
            if (_lvDetail.SelectedItems.Count > 0 && _lvDetail.SelectedItems[0].Tag is RidePart r) SelectRide(r);
        };
        _lvDetail.DoubleClick += (_, _) => OpenTrip(0);
        _btnTrip.Click += (_, _) => OpenTrip(0);
        _btnPrevRun.Click += (_, _) => OpenTrip(-1);
        _btnNextRun.Click += (_, _) => OpenTrip(1);
        _btnRefresh.Click += async (_, _) => await RefreshLiveAsync(_selected != null ? new[] { _selected } : _connections);

        _liveTimer.Tick += async (_, _) => await RefreshLiveAsync(_connections.Take(30));
        FormClosed += (_, _) => { _liveTimer.Dispose(); _api.Dispose(); };
    }

    private void SetNow()
    {
        var now = DateTime.Now;
        _dtpDate.Value = now.Date;
        _dtpTime.Value = now;
    }

    private void UpdatePlaceBoxes()
    {
        _txtFrom.Text = _from?.Name ?? "";
        _txtTo.Text = _to?.Name ?? "";
        _txtVia.Text = _via?.Name ?? "";
    }

    private void PickPlace(ref Place? target, string title)
    {
        // rank suggestions around the other endpoint if known (the app uses the phone's GPS position)
        var bias = _from ?? _to;
        using var dlg = new PlacePickerForm(_api, title, "", bias?.Lat, bias?.Lon);
        if (dlg.ShowDialog(this) == DialogResult.OK && dlg.SelectedPlace != null)
        {
            target = dlg.SelectedPlace;
            UpdatePlaceBoxes();
        }
    }

    // ============================================================== search & paging
    private SearchOptions BuildOptions() => new()
    {
        From = _from!, To = _to!, Via = _via,
        When = _dtpDate.Value.Date + new TimeSpan(_dtpTime.Value.Hour, _dtpTime.Value.Minute, 0),
        IsDeparture = _rbDep.Checked,
        OnlyDirect = _chkDirect.Checked,
        Train = _chkTrain.Checked, Bus = _chkBus.Checked, Tram = _chkTram.Checked, Trolley = _chkTrolley.Checked,
        Metro = _chkMetro.Checked, Cable = _chkCable.Checked, Ferry = _chkFerry.Checked,
        LowFloor = _chkLowFloor.Checked, Bike = _chkBike.Checked, Stroller = _chkStroller.Checked,
    };

    private async Task SearchAsync()
    {
        if (_from == null || _to == null)
        {
            SetStatus("Vyberte místo odjezdu a cíl.");
            return;
        }
        _opts = BuildOptions();
        _connections.Clear();
        _minIndex = 0;
        _maxIndex = 0;
        _selected = null;
        RefreshList();
        RenderDetail();
        await RunPageAsync(0, prepend: false);
        _liveTimer.Start();
    }

    private async Task LoadPageAsync(bool later)
    {
        if (_opts == null) return;
        if (later) await RunPageAsync(_maxIndex, prepend: false);
        else await RunPageAsync(_minIndex - _opts.Count, prepend: true);
    }

    private async Task RunPageAsync(int index, bool prepend)
    {
        if (_opts == null) return;
        _searchCts?.Cancel();
        var cts = _searchCts = new CancellationTokenSource();
        SetBusy(true, "Hledám spojení…");
        try
        {
            var res = await _api.SearchAsync(_opts, index, _connections.Select(c => c.Hash), cts.Token);
            if (cts.IsCancellationRequested) return;

            var fresh = res.Connections.Where(c => _connections.All(x => x.Hash != c.Hash)).ToList();
            _connections.AddRange(fresh);
            _connections.Sort((a, b) => a.Departure != b.Departure ? a.Departure.CompareTo(b.Departure) : a.Arrival.CompareTo(b.Arrival));

            if (prepend) _minIndex = index;
            else _maxIndex = index + _opts.Count;

            var keep = _selected;
            RefreshList();
            if (keep != null) _lstConn.SelectedItem = keep;
            else if (_lstConn.Items.Count > 0 && !prepend) _lstConn.SelectedIndex = 0;
            else if (prepend && fresh.Count > 0) _lstConn.TopIndex = 0;

            SetStatus(fresh.Count == 0 ? "Žádné další spoje." : $"Načteno {fresh.Count} spojení (celkem {_connections.Count}).");
            _ = RefreshLiveAsync(fresh);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            SetStatus("Chyba: " + ex.Message);
        }
        finally
        {
            SetBusy(false, null);
        }
    }

    /// <summary>gettripinfos for all rides of the given connections -> delays shown in the list/detail.</summary>
    private async Task RefreshLiveAsync(IEnumerable<Connection> conns)
    {
        var rides = conns.SelectMany(c => c.Rides).ToList();
        if (rides.Count == 0) return;
        try
        {
            for (int i = 0; i < rides.Count; i += 20)
            {
                var chunk = rides.Skip(i).Take(20).ToList();
                var infos = await _api.GetTripInfosAsync(chunk);
                for (int j = 0; j < chunk.Count && j < infos.Count; j++) chunk[j].LiveInfo = infos[j];
            }
            _lstConn.Invalidate();
            RenderDetail();
        }
        catch (Exception ex)
        {
            SetStatus("Zpoždění nelze načíst: " + ex.Message);
        }
    }

    private void SetBusy(bool busy, string? text)
    {
        _btnSearch.Enabled = !busy;
        _btnLater.Enabled = _btnEarlier.Enabled = !busy && _opts != null && _connections.Count > 0;
        UseWaitCursor = busy;
        if (text != null) SetStatus(text);
    }

    private void SetStatus(string text) => _status.Text = text;

    private void RefreshList()
    {
        _lstConn.BeginUpdate();
        _lstConn.Items.Clear();
        foreach (var c in _connections) _lstConn.Items.Add(c);
        _lstConn.EndUpdate();
    }

    // ============================================================== list drawing
    private void LstConn_DrawItem(object? sender, DrawItemEventArgs e)
    {
        e.DrawBackground();
        if (e.Index < 0 || _lstConn.Items[e.Index] is not Connection c) return;
        bool sel = (e.State & DrawItemState.Selected) != 0;
        var fg = sel ? SystemColors.HighlightText : SystemColors.WindowText;
        var fg2 = sel ? SystemColors.HighlightText : Color.DimGray;
        var g = e.Graphics;
        var b = e.Bounds;
        int x = b.X + 8, y = b.Y + 4, lh = Font.Height + 2;

        // day header when the date changes
        var prev = e.Index > 0 ? _lstConn.Items[e.Index - 1] as Connection : null;
        string day = prev == null || prev.Departure.Date != c.Departure.Date ? Fmt.DayHeader(c.Departure) + "   " : "";

        using var big = new Font("Segoe UI", 11.5f, FontStyle.Bold);
        var times = $"{Fmt.Time(c.Departure)} – {Fmt.Time(c.Arrival)}";
        TextRenderer.DrawText(g, times, big, new Point(x, y), fg);
        var right = $"{day}{Fmt.Duration(c.DurationSeconds)} · {Fmt.Transfers(c.TransferCount)}";
        var rsz = TextRenderer.MeasureText(right, Font);
        TextRenderer.DrawText(g, right, Font, new Point(b.Right - rsz.Width - 8, y + 3), fg2);

        // vehicles
        y += big.Height + 2;
        int cx = x;
        foreach (var part in c.Parts)
        {
            string label;
            Color color;
            switch (part)
            {
                case RidePart r:
                    label = $"{r.Type.CzechName()} {r.RouteName}";
                    color = VehicleColor(r.Type);
                    break;
                case WalkPart w:
                    label = $"pěšky {w.DistanceMeters} m";
                    color = Color.Gray;
                    break;
                default:
                    continue;
            }
            var sz = TextRenderer.MeasureText(label, Font);
            var rect = new Rectangle(cx, y, sz.Width + 6, lh);
            if (part is RidePart)
            {
                using var br = new SolidBrush(color);
                g.FillRectangle(br, rect);
                TextRenderer.DrawText(g, label, Font, rect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            }
            else TextRenderer.DrawText(g, label, Font, rect, fg2, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            cx += rect.Width + 6;
            if (cx > b.Right - 60) break;
        }

        // from -> to + delay
        y += lh + 2;
        var first = c.Rides.FirstOrDefault();
        var last = c.Rides.LastOrDefault();
        var fromTo = $"{first?.Boarding?.Stop.Name}{(string.IsNullOrEmpty(first?.StartPlatform) ? "" : $" (nást. {first!.StartPlatform})")} → {last?.Alighting?.Stop.Name}";
        int? maxDelay = c.Rides.Select(r => r.DelaySeconds).Where(d => d.HasValue).Select(d => d!.Value).DefaultIfEmpty(int.MinValue).Max();
        string? delay = maxDelay == int.MinValue ? null : Fmt.Delay(maxDelay);
        TextRenderer.DrawText(g, fromTo, Font, new Rectangle(x, y, b.Width - 150, lh), fg2, TextFormatFlags.EndEllipsis);
        if (delay != null)
        {
            var dsz = TextRenderer.MeasureText(delay, _bold);
            var dcol = sel ? SystemColors.HighlightText : maxDelay > 60 ? Color.Firebrick : Color.ForestGreen;
            TextRenderer.DrawText(g, delay, _bold, new Point(b.Right - dsz.Width - 8, y), dcol);
        }

        using var pen = new Pen(Color.Gainsboro);
        g.DrawLine(pen, b.Left, b.Bottom - 1, b.Right, b.Bottom - 1);
    }

    private static Color VehicleColor(TransportType t) => t switch
    {
        TransportType.Train => Color.FromArgb(0, 94, 168),
        TransportType.Bus => Color.FromArgb(0, 128, 96),
        TransportType.Tram => Color.FromArgb(200, 40, 40),
        TransportType.Trolley => Color.FromArgb(120, 60, 160),
        TransportType.Metro => Color.FromArgb(230, 120, 0),
        TransportType.Boat => Color.FromArgb(0, 150, 200),
        TransportType.Ropeway => Color.FromArgb(120, 90, 40),
        _ => Color.DimGray
    };

    // ============================================================== detail
    private void RenderDetail()
    {
        var c = _selected;
        _lvDetail.BeginUpdate();
        _lvDetail.Items.Clear();
        _lvDetail.Groups.Clear();

        if (c == null)
        {
            _lvDetail.EndUpdate();
            _lblHeader.Text = "";
            _txtNotes.Text = "";
            SelectRide(null);
            _btnRefresh.Enabled = false;
            return;
        }

        var firstRide = c.Rides.FirstOrDefault();
        var lastRide = c.Rides.LastOrDefault();
        _lblHeader.Text = $"{firstRide?.Boarding?.Stop.Name} {Fmt.Time(c.Departure)}  →  {lastRide?.Alighting?.Stop.Name} {Fmt.Time(c.Arrival)}\n" +
                          $"{Fmt.Duration(c.DurationSeconds)} · {Fmt.Transfers(c.TransferCount)} · {Fmt.DayHeader(c.Departure)}";

        var notes = new List<string>();
        ListViewItem? selectItem = null;

        for (int pi = 0; pi < c.Parts.Count; pi++)
        {
            switch (c.Parts[pi])
            {
                case RidePart r:
                {
                    var delay = Fmt.Delay(r.DelaySeconds);
                    var header = $"{r.Title}   ·   {r.Agency}   ·   {Fmt.Duration(r.Departure, r.Arrival)}{(delay != null ? "   ·   " + delay : "")}";
                    var grp = new ListViewGroup(header) { Tag = r };
                    _lvDetail.Groups.Add(grp);

                    var stops = _chkAllStops.Checked
                        ? r.RiddenStops.ToList()
                        : new[] { r.Boarding, r.Alighting }.Where(s => s != null).Cast<TripStop>().ToList();
                    for (int i = 0; i < stops.Count; i++)
                    {
                        var s = stops[i];
                        bool isFirst = i == 0, isLast = i == stops.Count - 1;
                        string platform = r.PlatformAt(s.Stop.Id) ?? s.Platform ?? "";
                        if (isFirst && platform.Length == 0) platform = r.StartPlatform ?? "";
                        if (isLast && platform.Length == 0) platform = r.EndPlatform ?? "";
                        var item = new ListViewItem(new[]
                        {
                            isFirst ? "" : Fmt.Time(s.Arrival),
                            isLast ? "" : Fmt.Time(s.Departure),
                            s.Stop.Name,
                            platform,
                            Fmt.StopNotes(s, r),
                        }, grp) { Tag = r };
                        if (isFirst || isLast) item.Font = _bold;
                        else item.ForeColor = Color.DimGray;
                        _lvDetail.Items.Add(item);
                        if (r == _selectedRide && isFirst) selectItem = item;
                    }

                    var rn = Fmt.RideNotes(r).ToList();
                    if (rn.Count > 0) { notes.Add(r.Title + ":"); notes.AddRange(rn); }
                    break;
                }
                case WalkPart w:
                {
                    var grp = new ListViewGroup($"Pěšky {w.DistanceMeters} m   ·   {Fmt.Duration(w.Departure, w.Arrival)}");
                    _lvDetail.Groups.Add(grp);
                    _lvDetail.Items.Add(new ListViewItem(new[] { Fmt.Time(w.Departure), Fmt.Time(w.Arrival), "přesun na další zastávku", "", "" }, grp)
                    { ForeColor = Color.Gray });
                    break;
                }
                case WaitPart wt:
                {
                    var grp = new ListViewGroup($"Přestup – čekání {Fmt.Duration(wt.Departure, wt.Arrival)}");
                    _lvDetail.Groups.Add(grp);
                    _lvDetail.Items.Add(new ListViewItem(new[] { Fmt.Time(wt.Departure), Fmt.Time(wt.Arrival), "", "", "" }, grp)
                    { ForeColor = Color.Gray });
                    break;
                }
            }
        }
        _lvDetail.EndUpdate();

        foreach (var p in c.Payments)
        {
            var price = p.Price is { } pr ? $"{pr:0.##} {(p.Currency == "CZK" ? "Kč" : p.Currency)}" : "cena u prodejce";
            var who = p.EshopName is { Length: > 0 } e ? e : p.SourceName ?? "";
            notes.Add($"Jízdenka ({(p.Type == "partialprice" ? "část trasy" : "celá trasa")}): {p.StopFrom} → {p.StopTo}: {price} {who} {p.Url}".TrimEnd());
        }
        _txtNotes.Text = string.Join(Environment.NewLine, notes);

        if (_selectedRide == null || !c.Rides.Contains(_selectedRide)) SelectRide(firstRide);
        else if (selectItem != null) selectItem.Selected = true;
        _btnRefresh.Enabled = true;
    }

    private void SelectRide(RidePart? r)
    {
        _selectedRide = r;
        _btnTrip.Enabled = _btnPrevRun.Enabled = _btnNextRun.Enabled = r != null;
        if (r != null)
        {
            _btnTrip.Text = $"Detail: {r.Type.CzechName()} {r.RouteName}";
        }
        else _btnTrip.Text = "Detail spoje";
    }

    /// <summary>offset 0 = the ride itself, -1/+1 = previous/next run of the same line ("swipe").</summary>
    private void OpenTrip(int offset)
    {
        if (_selectedRide == null) return;
        using var f = new TripForm(_api, _selectedRide, offset, _opts);
        f.ShowDialog(this);
    }
}
