# YFinanceFetcher.py
import yfinance as yf


def testVersion():
    try:
        print(f"yfinance version: {yf.__spec__}")
        print("Import successful!")
    except AttributeError:
        print("Import failed or yfinance is not properly installed.")


def fetch_equity_info(symbol):
    """Fetch equity metadata (name, currency, exchange, sector) for a given ticker.

    Returns a dict with keys: ticker, name, currency, exchange, sector.
    Raises KeyError / requests exceptions if yfinance cannot resolve the symbol.
    """
    info = yf.Ticker(symbol).info
    return {
        "ticker": symbol,
        "name": info.get("shortName", info.get("longName", "")),
        # currency is an ISO-4217 code, e.g. "USD", "EUR" — must match the C++ Currency enum.
        "currency": info.get("currency", ""),
        "exchange": info.get("exchange", ""),
        "sector": info.get("sector", ""),
    }


def equity_exists(symbol):
    """Return True if yfinance can resolve the symbol to a named security."""
    try:
        info = yf.Ticker(symbol).info
        return bool(info.get("shortName") or info.get("longName"))
    except Exception:
        return False


def fetch_ohlcv(symbol, start_iso, end_iso, interval):
    # auto_adjust=True (yfinance default): all OHLCV prices are adjusted for
    # splits and dividends. hist["Close"] is therefore the adjusted close —
    # suitable for return calculations. There is no separate "Adj Close" column;
    # the adjustment is applied in-place to Open/High/Low/Close.
    hist = yf.Ticker(symbol).history(
        start=start_iso, end=end_iso, interval=interval, auto_adjust=True
    )
    return {
        "timestamps_ms": (hist.index.astype("int64") // 10**6).tolist(),
        "open": hist["Open"].tolist(),
        "high": hist["High"].tolist(),
        "low": hist["Low"].tolist(),
        "close": hist["Close"].tolist(),
        "volume": hist["Volume"].tolist(),
    }
