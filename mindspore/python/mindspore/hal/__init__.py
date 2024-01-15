# Copyright 2023 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

"""
Python hardware abstract layer interfaces.

MindSpore export 'mindspore.hal' for users to access hardware resources and abilities.
"""
from mindspore.hal.device import is_initialized, is_available, device_count, get_device_capability,\
                                 get_device_properties, get_device_name, get_arch_list
from mindspore.hal.stream import Stream, synchronize, set_cur_stream, current_stream, default_stream,\
                                 StreamCtx
from mindspore.hal.event import Event

__all__ = [
    "is_initialized", "is_available", "device_count", "get_device_capability",
    "get_device_properties", "get_device_name", "get_arch_list",
    "Stream", "synchronize", "set_cur_stream", "current_stream", "default_stream", "StreamCtx", "Event"
]
