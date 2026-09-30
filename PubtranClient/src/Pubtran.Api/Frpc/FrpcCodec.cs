using System.Collections;
using System.Text;

namespace Pubtran.Api.Frpc;

/// <summary>
/// Struct in FastRPC = ordered string-keyed map. Insertion order is preserved on encode.
/// </summary>
public sealed class FrpcStruct : IEnumerable<KeyValuePair<string, object?>>
{
    private readonly List<KeyValuePair<string, object?>> _items = new();
    private readonly Dictionary<string, int> _index = new(StringComparer.Ordinal);

    public object? this[string key]
    {
        get => _index.TryGetValue(key, out var i) ? _items[i].Value : null;
        set
        {
            if (_index.TryGetValue(key, out var i)) _items[i] = new(key, value);
            else { _index[key] = _items.Count; _items.Add(new(key, value)); }
        }
    }

    /// <summary>Collection-initializer support: new FrpcStruct { { "a", 1 } }.</summary>
    public void Add(string key, object? value) => this[key] = value;

    public bool ContainsKey(string key) => _index.ContainsKey(key);
    public int Count => _items.Count;
    public IEnumerable<string> Keys => _items.Select(i => i.Key);

    public IEnumerator<KeyValuePair<string, object?>> GetEnumerator() => _items.GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    // ---- typed accessors (FastRPC ints decode as long, doubles as double) ----
    public string? GetString(string key, string? def = null) => this[key] as string ?? def;

    public long GetLong(string key, long def = 0) => this[key] switch
    {
        long l => l,
        int i => i,
        double d => (long)d,
        bool b => b ? 1 : 0,
        _ => def
    };

    public long? GetLongOrNull(string key) => ContainsKey(key) && this[key] is long or int ? GetLong(key) : null;
    public int GetInt(string key, int def = 0) => (int)GetLong(key, def);

    public double GetDouble(string key, double def = double.NaN) => this[key] switch
    {
        double d => d,
        long l => l,
        int i => i,
        _ => def
    };

    public bool GetBool(string key, bool def = false) => this[key] switch
    {
        bool b => b,
        long l => l != 0,
        _ => def
    };

    public FrpcStruct? GetStruct(string key) => this[key] as FrpcStruct;
    public List<object?> GetArray(string key) => this[key] as List<object?> ?? new List<object?>();
    public IEnumerable<FrpcStruct> GetStructs(string key) => GetArray(key).OfType<FrpcStruct>();
}

public sealed class FrpcFaultException : Exception
{
    public long Code { get; }
    public FrpcFaultException(long code, string message) : base($"FastRPC fault {code}: {message}") => Code = code;
}

/// <summary>
/// Encoder/decoder for Seznam FastRPC binary protocol (content type application/x-frpc-rest).
/// The app speaks protocol version 2.1: body = magic CA 11, version 02 01, then a single value
/// (for requests a struct of parameters, for responses the result struct).
/// </summary>
public static class FrpcCodec
{
    private const byte TInt = 1, TBool = 2, TDouble = 3, TString = 4, TDateTime = 5, TBinary = 6,
        TIntPos = 7, TIntNeg = 8, TStruct = 10, TArray = 11, TNull = 12, TMethodCall = 13,
        TMethodResponse = 14, TFault = 15;

    // ======================== ENCODE ========================

    public static byte[] Encode(object? value)
    {
        using var ms = new MemoryStream();
        ms.Write(new byte[] { 0xCA, 0x11, 0x02, 0x01 });
        WriteValue(ms, value);
        return ms.ToArray();
    }

    private static void WriteValue(Stream s, object? v)
    {
        switch (v)
        {
            case null:
                s.WriteByte(TNull << 3);
                break;
            case bool b:
                s.WriteByte((byte)((TBool << 3) | (b ? 1 : 0)));
                break;
            case byte or sbyte or short or ushort or int or uint or long:
                WriteInt(s, Convert.ToInt64(v));
                break;
            case double or float or decimal:
                s.WriteByte(TDouble << 3);
                s.Write(BitConverter.GetBytes(Convert.ToDouble(v)));
                break;
            case string str:
            {
                var bytes = Encoding.UTF8.GetBytes(str);
                WriteLengthHeader(s, TString, (ulong)bytes.Length);
                s.Write(bytes);
                break;
            }
            case DateTimeOffset dto:
                WriteDateTime(s, dto);
                break;
            case DateTime dt:
                WriteDateTime(s, new DateTimeOffset(dt));
                break;
            case byte[] bin:
                WriteLengthHeader(s, TBinary, (ulong)bin.Length);
                s.Write(bin);
                break;
            case FrpcStruct st:
                WriteLengthHeader(s, TStruct, (ulong)st.Count);
                foreach (var (key, val) in st)
                {
                    var k = Encoding.UTF8.GetBytes(key);
                    if (k.Length > 255) throw new ArgumentException("Struct key too long: " + key);
                    s.WriteByte((byte)k.Length);
                    s.Write(k);
                    WriteValue(s, val);
                }
                break;
            case IEnumerable seq:
            {
                var list = seq.Cast<object?>().ToList();
                WriteLengthHeader(s, TArray, (ulong)list.Count);
                foreach (var item in list) WriteValue(s, item);
                break;
            }
            default:
                throw new NotSupportedException("Cannot FastRPC-encode " + v.GetType());
        }
    }

    private static int ByteCount(ulong v)
    {
        int n = 1;
        while (n < 8 && (v >> (8 * n)) != 0) n++;
        return n;
    }

    private static void WriteLE(Stream s, ulong v, int n)
    {
        for (int i = 0; i < n; i++) s.WriteByte((byte)(v >> (8 * i)));
    }

    private static void WriteLengthHeader(Stream s, byte type, ulong length)
    {
        int n = ByteCount(length);
        s.WriteByte((byte)((type << 3) | (n - 1)));
        WriteLE(s, length, n);
    }

    private static void WriteInt(Stream s, long v)
    {
        // v2: positive ints = INT8P, negative = INT8N storing the magnitude
        ulong mag = v >= 0 ? (ulong)v : (ulong)(-(v + 1)) + 1;
        int n = ByteCount(mag);
        s.WriteByte((byte)(((v >= 0 ? TIntPos : TIntNeg) << 3) | (n - 1)));
        WriteLE(s, mag, n);
    }

    private static void WriteDateTime(Stream s, DateTimeOffset dto)
    {
        s.WriteByte(TDateTime << 3);
        s.WriteByte(unchecked((byte)(sbyte)(dto.Offset.TotalMinutes / 15))); // zone in quarter-hours (+02:00 -> 8)
        WriteLE(s, unchecked((uint)(int)dto.ToUnixTimeSeconds()), 4);
        var t = dto.DateTime; // wall-clock time in that offset
        ulong packed = (ulong)(uint)t.DayOfWeek
                       | (ulong)t.Second << 3
                       | (ulong)t.Minute << 9
                       | (ulong)t.Hour << 15
                       | (ulong)t.Day << 20
                       | (ulong)t.Month << 25
                       | (ulong)(uint)(t.Year - 1600) << 29;
        WriteLE(s, packed, 5);
    }

    // ======================== DECODE ========================

    public static object? Decode(byte[] data)
    {
        if (data.Length < 4 || data[0] != 0xCA || data[1] != 0x11)
            throw new FormatException("Not a FastRPC body");
        var r = new Reader(data, data[2]) { Pos = 4 };
        var value = r.ReadValue();
        if (value is FrpcFault f) throw new FrpcFaultException(f.Code, f.Message);
        return value;
    }

    private sealed record FrpcFault(long Code, string Message);

    private sealed class Reader
    {
        private readonly byte[] _d;
        private readonly int _major;
        public int Pos;
        public Reader(byte[] d, int major) { _d = d; _major = major; }

        private ulong U(int n)
        {
            ulong v = 0;
            for (int i = 0; i < n; i++) v |= (ulong)_d[Pos + i] << (8 * i);
            Pos += n;
            return v;
        }

        private byte[] Take(int n)
        {
            var b = new byte[n];
            Array.Copy(_d, Pos, b, 0, n);
            Pos += n;
            return b;
        }

        public object? ReadValue()
        {
            byte h = _d[Pos++];
            int type = h >> 3, info = h & 7;
            switch (type)
            {
                case TInt:
                    if (_major >= 3)
                    {
                        ulong z = U(info + 1);
                        return (long)(z >> 1) ^ -(long)(z & 1);
                    }
                    else
                    {
                        int n = info + 1;
                        ulong raw = U(n);
                        int shift = 64 - 8 * n;
                        return (long)(raw << shift) >> shift; // sign extend
                    }
                case TBool: return (info & 1) == 1;
                case TDouble: return BitConverter.ToDouble(Take(8));
                case TString: return Encoding.UTF8.GetString(Take((int)U(info + 1)));
                case TDateTime:
                {
                    sbyte zone = unchecked((sbyte)_d[Pos++]);
                    long ts = _major >= 3 ? (long)U(8) : unchecked((int)(uint)U(4));
                    Pos += 5; // packed wall-clock fields, redundant with ts
                    var offset = TimeSpan.FromMinutes(zone * 15);
                    return DateTimeOffset.FromUnixTimeSeconds(ts).ToOffset(offset);
                }
                case TBinary: return Take((int)U(info + 1));
                case TIntPos: return (long)U(info + 1);
                case TIntNeg: return -(long)U(info + 1);
                case TStruct:
                {
                    var n = (long)U(info + 1);
                    var st = new FrpcStruct();
                    for (long i = 0; i < n; i++)
                    {
                        int kl = _d[Pos++];
                        var key = Encoding.UTF8.GetString(Take(kl));
                        st[key] = ReadValue();
                    }
                    return st;
                }
                case TArray:
                {
                    var n = (long)U(info + 1);
                    var list = new List<object?>((int)Math.Min(n, 4096));
                    for (long i = 0; i < n; i++) list.Add(ReadValue());
                    return list;
                }
                case TNull: return null;
                case TMethodResponse: return ReadValue();
                case TFault:
                {
                    var code = ReadValue();
                    var msg = ReadValue();
                    return new FrpcFault(code is long l ? l : 0, msg as string ?? "");
                }
                case TMethodCall:
                    throw new NotSupportedException("Method call bodies are not expected in responses");
                default:
                    throw new FormatException($"Unknown FastRPC type {type} at offset {Pos - 1}");
            }
        }
    }
}
