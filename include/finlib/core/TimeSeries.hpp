//TimeSeries.hpp
#pragma once

#include<iostream>
#include<vector>
#include<stdexcept>
#include<algorithm>
#include<execution>
#include<functional>
#include<optional>

enum class InterpolationStrategy { Linear, Stochastic, Nearest };

class TimeSeries{
    private:
      std::shared_ptr<const std::vector<int64_t>> timestamps;
        std::vector<double> values;

        std::vector<double> partial_walk(const std::vector<int64_t>& target_timestamps, size_t start_idx, size_t end_idx,InterpolationStrategy strategy,std::optional<uint32_t> seed) const;
    public:

        TimeSeries(std::vector<int64_t> ts, std::vector<double> vals)
            : timestamps(std::make_shared<const std::vector<int64_t>>(std::move(ts))), values(std::move(vals)){
                if (timestamps->size()!=values.size()){
                    throw std::invalid_argument("Size mismatch between timestamps and values");
                }
            }

        TimeSeries(std::shared_ptr<const std::vector<int64_t>> ts, std::vector<double> vals)
            : timestamps(std::move(ts)), values(std::move(vals)){
                if (timestamps->size()!=values.size()){
                    throw std::invalid_argument("Size mismatch between timestamps and values");
                }
            }
        friend void swap(TimeSeries& first, TimeSeries& second) noexcept {
            std::swap(first.timestamps, second.timestamps);
            std::swap(first.values, second.values);
        }

        TimeSeries& operator=(TimeSeries other) noexcept {
            swap(*this, other);
            return *this;
        }
        TimeSeries(const TimeSeries& other) 
            : timestamps(other.timestamps), values(other.values) {}

        TimeSeries(TimeSeries&& other) noexcept 
            : timestamps(std::move(other.timestamps)), values(std::move(other.values)) {}

        ~TimeSeries() = default;

        //Accessors
        size_t size() const {return values.size();}
        const std::vector<double>& get_values() const {return values;}
        const std::vector<int64_t>& get_timestamps() const {return *timestamps;}
        const std::shared_ptr<const std::vector<int64_t>>& get_shared_timestamps() const { return timestamps; }

        //helper
        void verify_alignment(const TimeSeries& other) const;

        //Operator Overloading
        TimeSeries operator*(const TimeSeries& other) const;
        TimeSeries& operator*=(const TimeSeries& other);
        TimeSeries operator*(double scalar) const;
        TimeSeries& operator*=(double scalar);

        TimeSeries operator+(const TimeSeries& other) const;
        TimeSeries& operator+=(const TimeSeries& other);
        TimeSeries operator+(double scalar) const;
        TimeSeries& operator+=(double scalar);
        friend std::ostream& operator<<(std::ostream& os, const TimeSeries& obj);

        //Transformation Method
        TimeSeries resampling(const std::vector<int64_t>& targetTimestamps, InterpolationStrategy strategy, std::optional<uint32_t> seed = std::nullopt) const;
        TimeSeries shift(int lag) const;
        // TimeSeries shift(int lag) && ;

        TimeSeries addUnion(const TimeSeries& other) const;
        TimeSeries addIntersection(const TimeSeries& other) const;
        TimeSeries addResampling(const TimeSeries& other, InterpolationStrategy strategy);

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
};

inline std::ostream& operator<<(std::ostream& os, const TimeSeries& obj) {
    const auto& ts = obj.get_timestamps();
    const auto& vs = obj.get_values();

    os << "[\n";
    for (size_t i = 0; i < obj.size(); ++i) {
        os << "  [" << ts[i] << ", " << vs[i] << "]\n";
    }
    os << "]";
    return os;
}
