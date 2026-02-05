//TimeSeries.hpp
#pragma once

#include<iostream>
#include<vector>
#include<stdexcept>
#include<algorithm>
#include <execution>
#include<functional>

enum class InterpolationStrategy { Linear, Stochastic, Nearest };

class TimeSeries{
    private:
        std::vector<int64_t> timestamps;
        std::vector<double> values;

        std::vector<double> partial_walk(const std::vector<int64_t>& target_timestamps, size_t start_idx, size_t end_idx,InterpolationStrategy strategy) const;
    public:
        TimeSeries(std::vector<int64_t> ts, std::vector<double> vals)
            : timestamps(std::move(ts)), values(std::move(vals)){
                if (timestamps.size()!=values.size()){
                    throw std::invalid_argument("Size mismatch between timestamps and values");
                }
            }
            
        size_t size() const {return values.size();}
        const std::vector<double>& get_values() const {return values;}
        const std::vector<int64_t>& get_timestamps() const {return timestamps;}

        TimeSeries resampling(const std::vector<int64_t>& targetTimestamps, InterpolationStrategy strategy ) const;
        
        // TimeSeries shift(int lag) const & ;
        // TimeSeries shift(int lag) && ;

        TimeSeries addUnion(const TimeSeries& other) const;
        TimeSeries addIntersection(const TimeSeries& other) const;
        TimeSeries addResampling(const TimeSeries& other, InterpolationStrategy strategy);

        TimeSeries operator*(const TimeSeries& other) const;
        TimeSeries operator*(double scalar) const;


        template <typename Func>
        TimeSeries apply(Func func) const & {
            //Create a new Time-series Object
            std::vector<double> new_vals(values.size());
            if (values.size() < 1000) {
                std::transform(values.begin(), values.end(), new_vals.begin(), func);
            } else {
                std::transform(std::execution::par, values.begin(), values.end(), new_vals.begin(), func);
            }
            return TimeSeries(this->timestamps, std::move(new_vals));
        }

        template <typename Func>
        TimeSeries apply(Func func) && {
            //In place but return a temporary
            if (values.size() < 1000) {
                std::transform(values.begin(), values.end(), values.begin(), func);
            } else {
                std::transform(std::execution::par, values.begin(), values.end(), values.begin(), func);
            }
            return std::move(*this);
        }
        
        template <typename Func>
        TimeSeries& applyInPlace(Func func) {
            if (values.size() < 1000) {
                std::transform(values.begin(), values.end(), values.begin(), func);
            } else {
                std::transform(std::execution::par, values.begin(), values.end(), values.begin(), func);
            }
            return *this; // Return the current object by reference
        }

        friend std::ostream& operator<<(std::ostream& os, const TimeSeries& obj);

};

inline  std::ostream& operator<<(std::ostream& os, const TimeSeries& obj) {
    os << "[ ";
    for (size_t i = 0; i < obj.size(); ++i) {
        os << "[ " << obj.timestamps[i] << " , " << obj.values[i]<< " ]"<<std::endl;
    }
    os << "]";
    return os;
}
