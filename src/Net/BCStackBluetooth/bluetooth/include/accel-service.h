/*
  Copyright 2013-2014 bcstack.org

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

#ifndef _ACCEL_SERVICE_H_
#define _ACCEL_SERVICE_H_

extern const bt_gatt_service_t bt_accel_service;
void accel_set_xyz(u8 x, u8 y, u8 z);

#endif // _ACCEL_SERVICE_H_
