#include <algorithm>
#include <chrono>
#include <ios>
#include <iostream>
#include <ostream>
#include <random>
#include <utility>
#include <vector>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <boost/container/small_vector.hpp>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <glm/gtx/norm.hpp>


namespace
{
    struct CollisionHash
    {
        void feed(std::size_t i, std::size_t j)
        {
            hash_ += static_cast<std::uint64_t>(i * j);
            count_ += 1;
        }

        std::uint64_t value() const
        {
            return hash_ * count_;
        }

        std::uint64_t count() const
        {
            return count_;
        }

      private:
        std::uint64_t hash_ = 0;
        std::uint64_t count_ = 0;
    };

    std::ostream& operator<<(std::ostream& os, CollisionHash const& hash)
    {
        return os << std::dec << hash.count() << ':'
                  << std::hex << hash.value()
                  << std::dec;
    }
}

namespace
{
    using Point = glm::dvec2;
    using Vector = glm::dvec2;

    struct HashedBins
    {
        HashedBins(std::size_t nbins, double dcut)
            : bins_(nbins), dcut_{dcut}, freq_{1 / dcut}
        {
            for (std::size_t dx = nbins - 1; dx <= nbins + 1; ++dx) {
                for (std::size_t dy = nbins - 1; dy <= nbins + 1; ++dy) {
                    deltas_.push_back(hash(dx, dy));
                }
            }

            std::sort(deltas_.begin(), deltas_.end());
            deltas_.erase(std::unique(deltas_.begin(), deltas_.end()),
                          deltas_.end());
        }

        CollisionHash collision_hash(std::vector<Point> const& points)
        {
            for (Bin& bin : bins_) {
                bin.members.clear();
            }

            for (std::size_t index = 0; index < points.size(); ++index) {
                bins_[locate_bin(points[index])].members.push_back(index);
            }

            CollisionHash hash;

            double const dcut2 = dcut_ * dcut_;

            for (std::size_t center = 0; center < bins_.size(); ++center) {
                for (std::size_t delta : deltas_) {
                    std::size_t const other = (center + delta) % bins_.size();

                    for (std::size_t i : bins_[other].members) {
                        for (std::size_t j : bins_[center].members) {
                            if (i < j && glm::distance2(points[i], points[j]) < dcut2) {
                                hash.feed(i, j);
                            }
                        }
                    }
                }
            }

            return hash;
        }

      private:
        static std::size_t const x_stride = 33554393; // prev prime of 2^25
        static std::size_t const y_stride = 33554467; // next prime of 2^25

        std::size_t locate_bin(Point point) const
        {
            double const x_offset = static_cast<double>(x_stride / 2);
            double const y_offset = static_cast<double>(y_stride / 2);

            std::size_t const x = static_cast<std::size_t>(std::round(freq_ * point.x) + x_offset);
            std::size_t const y = static_cast<std::size_t>(std::round(freq_ * point.y) + y_offset);

            return hash(x, y) % bins_.size();
        }

        std::size_t hash(std::size_t x, std::size_t y) const
        {
            return x * x_stride + y * y_stride;
        }

      private:
        struct Bin
        {
            std::vector<std::size_t> members;
        };

        std::vector<Bin>         bins_;
        double                   dcut_;
        double                   freq_;
        std::vector<std::size_t> deltas_;
    };

    struct UniformBins
    {
        explicit
        UniformBins(double dcut)
            : dcut_{dcut}, freq_{1 / dcut}
        {
        }

        CollisionHash collision_hash(std::vector<Point> const& points)
        {
            Point min_point = points.front();
            Point max_point = points.front();

            for (Point const& point : points) {
                min_point.x = std::min(min_point.x, point.x);
                min_point.y = std::min(min_point.y, point.y);
                max_point.x = std::max(max_point.x, point.x);
                max_point.y = std::max(max_point.y, point.y);
            }

            set_domain(min_point, max_point);
            register_points(points);
            return detect_collisions(points);
        }

      private:
        void set_domain(Point min, Point max)
        {
            min_point_ = min;
            max_point_ = max;

            double const x_stride = max_point_.x - min_point_.x;
            double const y_stride = max_point_.y - min_point_.y;

            nx_ = static_cast<std::size_t>(freq_ * x_stride) + 1;
            ny_ = static_cast<std::size_t>(freq_ * y_stride) + 1;
        }

        void register_points(std::vector<Point> const& points)
        {
            bins_.resize(nx_ * ny_);

            for (Bin& bin : bins_) {
                bin.members.clear();
            }

            for (std::size_t index = 0; index < points.size(); ++index) {
                Point const point = points[index];
                bins_[locate(point)].members.push_back(index);
            }
        }

        CollisionHash detect_collisions(std::vector<Point> const& points) const
        {
            CollisionHash hash;

            std::size_t const nbins = bins_.size();
            std::size_t const deltas[] = {
                -1 - nx_ + nbins,
                 0 - nx_ + nbins,
                +1 - nx_ + nbins,

                -1 + nbins,
                 0,
                +1,

                -1 + nx_,
                 0 + nx_,
                +1 + nx_,
            };
            double const dcut2 = dcut_ * dcut_;

            for (std::size_t center = 0; center < nbins; ++center) {
                for (std::size_t delta : deltas) {
                    std::size_t const other = (center + delta) % nbins;
                    for (std::size_t i : bins_[center].members) {
                        for (std::size_t j : bins_[other].members) {
                            if (i >= j) {
                                continue;
                            }

                            if (glm::distance2(points[i], points[j]) < dcut2) {
                                hash.feed(i, j);
                            }
                        }
                    }
                }
            }

            return hash;
        }

        std::size_t locate(Point point)
        {
            std::size_t const x = static_cast<std::size_t>(freq_ * (point.x - min_point_.x));
            std::size_t const y = static_cast<std::size_t>(freq_ * (point.y - min_point_.y));

            return x + y * nx_;
        }

      private:
        struct Bin
        {
            boost::container::small_vector<std::size_t, 8> members;
        };

        std::vector<Bin> bins_;
        double           dcut_;
        double           freq_;
        Point            min_point_;
        Point            max_point_;
        std::size_t      nx_ = 0;
        std::size_t      ny_ = 0;
    };
}

namespace
{
    CollisionHash brute_force(std::vector<Point> const& points, double radius)
    {
        double const dcut = 2 * radius;
        double const dcut2 = dcut * dcut;

        CollisionHash hash;

        for (std::size_t j = 0; j < points.size(); ++j) {
            for (std::size_t i = 0; i < j; ++i) {
                if (glm::distance2(points[i], points[j]) < dcut2) {
                    hash.feed(i, j);
                }
            }
        }

        return hash;
    }

    CollisionHash uniform_bins(std::vector<Point> const& points, double radius)
    {
        return UniformBins{2 * radius}.collision_hash(points);
    }

    CollisionHash spatial_hashing(std::vector<Point> const& points, double radius)
    {
        std::size_t const nbins_heuristic = points.size() / 10;
        return HashedBins{nbins_heuristic, 2 * radius}.collision_hash(points);
    }
}

namespace
{
    double halton(unsigned long i, unsigned long radix)
    {
        double result = 0;
        double fraction = 1;

        for (; i != 0; i /= radix)
        {
            fraction /= radix;
            result += fraction * static_cast<double>(i % radix);
        }

        return result;
    }

    template<typename RNG>
    std::vector<Point> generate_uniform_points(std::size_t n_points, RNG& engine)
    {
        std::vector<Point> points;
        std::uniform_real_distribution<double> coord{-1, 1};

        for (std::size_t i = 0; i < n_points; ++i) {
            double const x = coord(engine);
            double const y = coord(engine);
            points.emplace_back(x, y);
        }

        return points;
    }

    std::vector<Point> generate_quasirandom_points(std::size_t n_points)
    {
        std::vector<Point> points;

        for (std::size_t i = 0; i < n_points; ++i) {
            double const x = 2 * halton(i, 2) - 1;
            double const y = 2 * halton(i, 3) - 1;
            points.emplace_back(x, y);
        }

        return points;
    }

    template<typename RNG>
    std::vector<Point> generate_bounded_uniform_points(std::size_t n_points, RNG& engine)
    {
        std::vector<Point> points;
        std::uniform_real_distribution<double> coord{-1, 1};

        while (points.size() < n_points) {
            double const x = coord(engine);
            double const y = coord(engine);
            if (x * x + y * y > 1) {
                continue;
            }
            points.emplace_back(x, y);
        }

        return points;
    }

    template<typename RNG>
    std::vector<Point> generate_normal_points(std::size_t n_points, RNG& engine)
    {
        std::vector<Point> points;
        std::normal_distribution<double> coord{0, 0.5};

        for (std::size_t i = 0; i < n_points; ++i) {
            double const x = coord(engine);
            double const y = coord(engine);
            points.emplace_back(x, y);
        }

        return points;
    }

    template<typename RNG>
    std::vector<Point> generate_random_walk(std::size_t n_points, double stride, RNG& engine)
    {
        std::vector<Point> points;

        points.emplace_back(); // initial point

        std::normal_distribution<double> normal;
        for (std::size_t i = 0; i < n_points; ++i) {
            Vector direction = {normal(engine), normal(engine)};
            direction /= direction.length();
            points.push_back(points.back() + stride * direction);
        }

        return points;
    }
}

namespace
{
    template<typename Rep, typename Period>
    double to_seconds(std::chrono::duration<Rep, Period> dur)
    {
        return std::chrono::duration_cast<std::chrono::duration<double>>(dur).count();
    }
}

int main()
{
    std::size_t const n_points = 100000;
    double const radius = 0.001;

    std::mt19937_64 engine;
    std::vector<Point> points;

    auto const bench = [&](char const* name, auto func) {
        using Clock = std::chrono::steady_clock;

        Clock::time_point const start = Clock::now();
        CollisionHash const hash = func(points, radius);
        Clock::duration const elapsed = Clock::now() - start;
        double const elapsed_ms = 1000 * to_seconds(elapsed);

        std::cout << name
                  << ":\t"
                  << hash
                  << '\t'
                  << elapsed_ms << " ms"
                  << '\n';
    };

    std::cout << "Uniform\n";
    points = generate_uniform_points(n_points, engine);
    bench("Brute", brute_force);
    bench("Bin", uniform_bins);
    bench("Hash", spatial_hashing);

    std::cout << "\nQuasirandom\n";
    points = generate_quasirandom_points(n_points);
    bench("Brute", brute_force);
    bench("Bin", uniform_bins);
    bench("Hash", spatial_hashing);

    std::cout << "\nNormal\n";
    points = generate_normal_points(n_points, engine);
    bench("Brute", brute_force);
    bench("Bin", uniform_bins);
    bench("Hash", spatial_hashing);

    std::cout << "\nBounded\n";
    points = generate_bounded_uniform_points(n_points, engine);
    bench("Brute", brute_force);
    bench("Bin", uniform_bins);
    bench("Hash", spatial_hashing);

    std::cout << "\nRandom walk\n";
    points = generate_random_walk(n_points, 2 * radius, engine);
    bench("Brute", brute_force);
    bench("Bin", uniform_bins);
    bench("Hash", spatial_hashing);
}
