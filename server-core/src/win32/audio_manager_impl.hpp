/*
   Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef _AUDIO_MANAGER_HPP
#define _AUDIO_MANAGER_HPP

#ifdef WIN32

#include <sdkddkver.h>

#include <mmreg.h>
#include <mmdeviceapi.h>

class network_manager;

namespace detail {

class audio_manager_impl {
protected:
};

}

#endif // WIN32
#endif // !_AUDIO_MANAGER_HPP