/*
 * Copyright (C) 2023 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "common/Singleton.h"

#include <opentelemetry/metrics/provider.h>

namespace zil {
namespace metrics {

namespace common = opentelemetry::common;
namespace metrics_api = opentelemetry::metrics;

class Logging : public Singleton<Logging> {
 public:
  Logging();
};

}  // namespace metrics
}  // namespace zil
