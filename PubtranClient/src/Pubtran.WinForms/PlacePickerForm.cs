using Pubtran.Api;

namespace Pubtran.WinForms;

/// <summary>
/// "Odkud" / "Kam" picker: empty text = recent places, typing = live suggestions (api/v1/suggest).
/// </summary>
internal sealed class PlacePickerForm : Form
{
    private readonly PubtranApi _api;
    private readonly double? _lat, _lon;
    private readonly TextBox _txt = new() { Dock = DockStyle.Top, Font = new Font("Segoe UI", 12f) };
    private readonly Label _hdr = new() { Dock = DockStyle.Top, Height = 26, ForeColor = Color.DimGray, Padding = new Padding(2, 6, 0, 0) };
    private readonly ListBox _list = new() { Dock = DockStyle.Fill, DrawMode = DrawMode.OwnerDrawFixed, IntegralHeight = false, BorderStyle = BorderStyle.None };
    private readonly System.Windows.Forms.Timer _debounce = new() { Interval = 300 };
    private CancellationTokenSource? _cts;

    public Place? SelectedPlace { get; private set; }

    public PlacePickerForm(PubtranApi api, string title, string initialText, double? lat, double? lon)
    {
        _api = api; _lat = lat; _lon = lon;
        Text = title;
        Font = new Font("Segoe UI", 9.5f);
        Size = new Size(480, 560);
        MinimumSize = new Size(360, 300);
        StartPosition = FormStartPosition.CenterParent;
        ShowInTaskbar = false;
        MinimizeBox = false;
        KeyPreview = true;

        _list.ItemHeight = (int)(Font.Height * 2.6);

        var buttons = new FlowLayoutPanel { Dock = DockStyle.Bottom, FlowDirection = FlowDirection.RightToLeft, Height = 40, Padding = new Padding(4) };
        var btnCancel = new Button { Text = "Zrušit", DialogResult = DialogResult.Cancel, AutoSize = true };
        var btnOk = new Button { Text = "Vybrat", AutoSize = true };
        btnOk.Click += (_, _) => Pick();
        buttons.Controls.AddRange(new Control[] { btnCancel, btnOk });
        CancelButton = btnCancel;

        Controls.Add(_list);
        Controls.Add(_hdr);
        Controls.Add(_txt);
        Controls.Add(buttons);

        _txt.Text = initialText;
        _txt.TextChanged += (_, _) => { _debounce.Stop(); _debounce.Start(); };
        _txt.KeyDown += Txt_KeyDown;
        _debounce.Tick += async (_, _) => { _debounce.Stop(); await RefreshListAsync(); };
        _list.DrawItem += List_DrawItem;
        _list.DoubleClick += (_, _) => Pick();
        _list.KeyDown += (_, e) => { if (e.KeyCode == Keys.Enter) { Pick(); e.Handled = true; } };

        Shown += async (_, _) => { _txt.SelectAll(); _txt.Focus(); await RefreshListAsync(); };
        FormClosed += (_, _) => { _cts?.Cancel(); _debounce.Dispose(); };
    }

    private void Txt_KeyDown(object? sender, KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Down && _list.Items.Count > 0)
        {
            _list.Focus();
            if (_list.SelectedIndex < 0) _list.SelectedIndex = 0;
            e.Handled = true;
        }
        else if (e.KeyCode == Keys.Enter)
        {
            if (_list.SelectedIndex < 0 && _list.Items.Count > 0) _list.SelectedIndex = 0;
            Pick();
            e.Handled = e.SuppressKeyPress = true;
        }
    }

    private async Task RefreshListAsync()
    {
        _cts?.Cancel();
        var q = _txt.Text.Trim();
        if (q.Length == 0)
        {
            var recent = RecentPlaces.Load();
            _hdr.Text = recent.Count > 0 ? "Nedávno hledaná místa" : "Začněte psát název obce, zastávky nebo adresy";
            Fill(recent);
            return;
        }

        var cts = _cts = new CancellationTokenSource();
        _hdr.Text = "Hledám…";
        try
        {
            var res = await _api.SuggestAsync(q, _lat, _lon, 10, cts.Token);
            if (cts.IsCancellationRequested) return;
            _hdr.Text = res.Count > 0 ? "Návrhy" : "Nic nenalezeno";
            Fill(res);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            if (!cts.IsCancellationRequested) _hdr.Text = "Chyba: " + ex.Message;
        }
    }

    private void Fill(IEnumerable<Place> places)
    {
        _list.BeginUpdate();
        _list.Items.Clear();
        foreach (var p in places) _list.Items.Add(p);
        _list.EndUpdate();
        if (_list.Items.Count > 0) _list.SelectedIndex = 0;
    }

    private void Pick()
    {
        if (_list.SelectedItem is not Place p) return;
        SelectedPlace = p;
        RecentPlaces.Add(p);
        DialogResult = DialogResult.OK;
        Close();
    }

    private void List_DrawItem(object? sender, DrawItemEventArgs e)
    {
        e.DrawBackground();
        if (e.Index < 0 || _list.Items[e.Index] is not Place p) return;
        bool sel = (e.State & DrawItemState.Selected) != 0;
        var fg = sel ? SystemColors.HighlightText : SystemColors.WindowText;
        var fg2 = sel ? SystemColors.HighlightText : Color.DimGray;
        var b = e.Bounds;

        // small colored tag: city / stop / other
        var tagColor = p.Source switch { "muni" => Color.SteelBlue, "pubt" => Color.SeaGreen, _ => Color.Gray };
        var tagRect = new Rectangle(b.X + 6, b.Y + b.Height / 2 - 7, 14, 14);
        using (var br = new SolidBrush(tagColor)) e.Graphics.FillEllipse(br, tagRect);

        using var bold = new Font(Font, FontStyle.Bold);
        int x = b.X + 28;
        TextRenderer.DrawText(e.Graphics, p.Name, bold, new Point(x, b.Y + 3), fg);
        var second = string.IsNullOrEmpty(p.Description) ? Fmt.PlaceKind(p) : p.Description;
        TextRenderer.DrawText(e.Graphics, second, Font, new Point(x, b.Y + 3 + bold.Height), fg2);
        e.DrawFocusRectangle();
    }
}
