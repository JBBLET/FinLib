# YFinance Funds Info Fetcher
import yfinance as yf


def testVersion():
    try:
        print(f"yfinance  version: {yf.__spec__}")
        print("Import successful!")
    except AttributeError:
        print("Import failed or yfinance is not properly installed")


def fetch_equity_info(symbol):
    """Fetch equity metadata (name, currency, exchange, sector) for a given ticker.

    Returns a dict with keys: ticker, name, currency, exchange, sector.
    Raises KeyError / requests exceptions if yfinance cannot resolve the symbol.
    """
    info = yf.Ticker(symbol).info
    for key in list(info.keys()):
        print(f"{key}: {info.get(key, '')}")


def fetch_fund_info(symbol: str):
    print("-----------------------------")
    print(f"Fund Info Ticker: {symbol}")
    print("-----------------------------")
    ticker = yf.Ticker(symbol)
    funds = ticker.funds_data
    print(f"Asset Classes: {funds.asset_classes}")
    print("---")
    print(f"Bonds Holdings: {funds.bond_holdings}")
    print("---")
    print(f"Bond Rating: {funds.bond_ratings}")
    print("---")
    print(f"Description: {funds.description}")
    print("---")
    print(f"Equity Holdings: {funds.equity_holdings}")
    print("---")
    print(f"Fund Operations: {funds.fund_operations}")
    print("---")
    print(f"Funds Overview: {funds.fund_overview}")
    print("---")
    print(f"Sector Weightings: {funds.sector_weightings}")
    print("---")
    print(f"Top Holdings: {funds.top_holdings}")
    print("---")


if __name__ == "__main__":
    symbol = "AAPL"
    fetch_equity_info(symbol)
    fetch_fund_info(symbol)
