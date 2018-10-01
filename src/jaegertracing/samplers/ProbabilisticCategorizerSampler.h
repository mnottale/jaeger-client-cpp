/*
 * Copyright (c) 2017 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JAEGERTRACING_SAMPLERS_PROBABILISTICCATEGORIZERSAMPLER_H
#define JAEGERTRACING_SAMPLERS_PROBABILISTICCATEGORIZERSAMPLER_H

#include "jaegertracing/Constants.h"
#include "jaegertracing/Tag.h"
#include "jaegertracing/TraceID.h"
#include "jaegertracing/samplers/Sampler.h"
#include "jaegertracing/samplers/SamplingStatus.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace jaegertracing {
namespace samplers {

class ProbabilisticCategorizerSampler : public Sampler {
  public:
    explicit ProbabilisticCategorizerSampler(std::vector<std::pair<std::string, double>> samplingRates)
        : _tags({ { kSamplerTypeTagKey, kSamplerTypeProbabilistic },
        })
    {
      _defaultSamplingBoundary = 0;
      for (auto const& sr: samplingRates)
      {
        auto bound = computeSamplingBoundary(sr.second);
        _samplingBoundaries.push_back(std::make_pair(sr.first, bound));
        if (sr.first == "default")
          _defaultSamplingBoundary = bound;
      }
    }

    SamplingStatus isSampled(const TraceID& id,
                             const std::string& operation) override
    {
      for (auto const& sr: _samplingBoundaries)
      {
        if (sr.first.size() <= operation.size() && std::equal(sr.first.begin(), sr.first.end(), operation.begin()))
        {
          return SamplingStatus(sr.second >= id.low(), _tags);
        }
      }
      return SamplingStatus(_defaultSamplingBoundary >= id.low(), _tags);
    }

    void close() override {}

    Type type() const override { return Type::kProbabilisticCategorizerSampler; }

  private:
    static constexpr auto kMaxRandomNumber =
        std::numeric_limits<uint64_t>::max();

    std::vector<std::pair<std::string, uint64_t>> _samplingBoundaries;
    uint64_t _defaultSamplingBoundary;
    std::vector<Tag> _tags;

    static uint64_t computeSamplingBoundary(long double samplingRate)
    {
        const auto maxRandNumber = static_cast<long double>(kMaxRandomNumber);
        const auto samplingBoundary = samplingRate * maxRandNumber;

        // Protect against overflow in case samplingBoundary rounds
        // higher than kMaxRandNumber.
        if (samplingBoundary == maxRandNumber) {
            return kMaxRandomNumber;
        }

        return static_cast<uint64_t>(samplingBoundary);
    }
};

}  // namespace samplers
}  // namespace jaegertracing

#endif  // JAEGERTRACING_SAMPLERS_PROBABILISTICCATEGORIZERSAMPLER_H
