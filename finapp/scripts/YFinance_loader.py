import sys
import yfinance as yf
import pandas as pd


def main():
    if len(sys.argv) != 5:
        print("Usage: yfinance_loader.py SYMBOL START END", file=sys.stderr)
        sys.exit(1)

    symbol = sys.argv[1]
    start = sys.argv[2]  # e.g. 2023-01-01
    end = sys.argv[3]  # e.g. 2024-01-01
    interval = sys.argv[4]

    df = yf.download(symbol, start=start, end=end, interval=interval, progress=False)

    if df.empty:
        return

    # Flatten yfinance multi-index columns
    if isinstance(df.columns, pd.MultiIndex):
        df.columns = df.columns.get_level_values(0)

    df = df.reset_index()

    # Date → epoch ms
    dt = pd.to_datetime(df["Date"], utc=True)
    ts = pd.to_datetime(dt, utc=True).astype("int64") // 10**6

    prices = df["Close"].astype("float64")

    print("timestamp,value")
    for t, v in zip(ts, prices):
        print(f"{t},{v}")


if __name__ == "__main__":
    main()
