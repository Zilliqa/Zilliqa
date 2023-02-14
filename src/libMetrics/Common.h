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

#ifndef ZILLIQA_SRC_LIBMETRICS_COMMON_H_
#define ZILLIQA_SRC_LIBMETRICS_COMMON_H_

#include <string>

namespace zil {
namespace metrics {

const std::string METRIC_FAMILY{"zilliqa"};
const std::string METRIC_SCHEMA_VERSION{"1.2.0"};
const std::string METRIC_SCHEMA{"https://opentelemetry.io/schemas/1.2.0"};

}  // namespace metrics
}  // namespace zil

#endif  // ZILLIQA_SRC_LIBMETRICS_COMMON_H_
