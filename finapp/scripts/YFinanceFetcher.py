# yfinance.py
import yfinance as yf


def testVersion():
    try:
        print(f"yfinance version: {yf.__spec__}")
        print("Import successful!")
    except AttributeError:
        print("Import failed or yfinance is not properly installed.")


def fetch_ohlcv(symbol, start_iso, end_iso, interval):
    hist = yf.Ticker(symbol).history(start=start_iso, end=end_iso, interval=interval)
    return {
        "timestamps_ms": (hist.index.astype("int64") // 10**6).tolist(),
        "open": hist["Open"].tolist(),
        "high": hist["High"].tolist(),
        "low": hist["Low"].tolist(),
        "close": hist["Close"].tolist(),
        "volume": hist["Volume"].tolist(),
    }
