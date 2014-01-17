/*
 * as.c
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
 *     Created on: Nov 18, 2013
 *         Author: Juan Carlos Dueñas López
 *          Email: juancarlos.duenas@centeropenmiddleware.com
 */

#ifndef AS_H_
#define AS_H_

typedef void* as;   /* Tipo abstracto que representa la tabla
                       asociativa y que contiene un puntero
                       a la información necesaria.  */

int as_crear(as *pt, size_t nreg, size_t lvalor);
    /*	 Crea una tabla en memoria, dimensionada para "nreg" registros.
	 El valor retornado es 1 si no falla y 0 si falla.
	 Los registros son de longitud variable "lvalor" octetos */

int as_cerrar(as t);
    /*   Cerrar la tabla, liberando los recursos asignados a ella.
         El valor retornado es 1 si no falla y 0 si falla.  */

int as_leer(as t, const size_t pos, void* pv);
    /*   Lee el valor almacenado en la posicion pos. Si no existe
	 el registro, falla.
         El valor retornado es 1 si no falla y 0 si falla.  */

int as_escribir(as t, const size_t pos, const void* pv);
    /*   Escribe un registro, dada su posicion y el valor asociado.
         Si existe el valor en esa posicion, lo reescribe.
         El valor retornado es 1 si no falla y 0 si falla.  */

int as_borrar(as t, const size_t pos);
    /*   Borra un registro en la posicion pos.  El valor retornado es
         siempre 1 (suponemos que no falla).  */

int as_llenos(as t);
/*   Devuelve el numero de registros almacenados  */

#endif /* AS_H_ */
