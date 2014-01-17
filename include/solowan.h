/*
 * solowan.h
 *
 *     Copyright 2014 Universidad Politécnica de Madrid - Center for Open Middleware
 *     (http://www.centeropenmiddleware.com)
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 *     Created on: Dec 12, 2013
 *         Author: Mattia Peirano
 *          Email: mattia.peirano[at]centeropenmiddleware.com
 */

#ifndef SOLOWAN_H_
#define SOLOWAN_H_

int optimize(hashtable ht, unsigned char *in_packet, uint16_t in_packet_size, unsigned char *out_packet, uint16_t *out_packet_size);
int deoptimize(hashtable as, unsigned char *input_packet_ptr, uint16_t input_packet_size, unsigned char *regenerated_packet, uint16_t *output_packet_size);
int cache(hashtable ht, unsigned char *packet_ptr, uint16_t packet_size);
#endif /* SOLOWAN_H_ */
