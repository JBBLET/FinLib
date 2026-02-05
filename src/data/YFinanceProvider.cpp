#include "finlib/data/implementation/YFinanceProvider.hpp"
#include "finlib/common/utils/time.hpp"

using common::utils::time::ts_to_date;

TimeSeries YFinanceProvider::load(
    const std::string& symbol,
    int64_t start_ts,
    int64_t end_ts
) {
    std::string start = ts_to_date(start_ts);
    std::string end   = ts_to_date(end_ts);

    std::string cmd =
        python_ + " " + script_path_ + " \"" +
        symbol + "\" \"" +
        start + "\" \"" +
        end + "\" \"1d\"";

    std::cerr << "CMD: " << cmd << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run yfinance loader");
    }

    std::vector<int64_t> timestamps;
    std::vector<double> values;

    char buffer[256];
    bool skip_header = true;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (skip_header) {
            skip_header = false;
            continue;
        }

        int64_t ts;
        double val;
        if (sscanf(buffer, "%ld,%lf", &ts, &val) == 2) {
            timestamps.push_back(ts);
            values.push_back(val);
        }
    }

    pclose(pipe);

    return TimeSeries(std::move(timestamps), std::move(values));
}
