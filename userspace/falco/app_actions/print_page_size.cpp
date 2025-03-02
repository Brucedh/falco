/*
Copyright (C) 2022 The Falco Authors.

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

#include "application.h"

using namespace falco::app;

application::run_result application::print_page_size()
{
	if(m_options.print_page_size)
	{
		long page_size = getpagesize();
		if(page_size <= 0)
		{
			return run_result::fatal("\nUnable to get the system page size through 'getpagesize()'\n");
		}
		else
		{
			falco_logger::log(LOG_INFO, "Your system page size is: " + std::to_string(page_size) + " bytes.\n");
		}
		return run_result::exit();
	}
	return run_result::ok();
}
