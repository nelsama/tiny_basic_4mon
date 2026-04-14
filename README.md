# Manual de Usuario: 6502 Tiny BASIC (Nexys Edition)
**Versión:** 3.5.6  
**Plataforma:** FPGA Nexys (Arquitectura 6502)

Este es un intérprete de BASIC minimalista diseñado para correr en un procesador 6502 implementado en FPGA. Permite el control directo de hardware, gestión de memoria y lógica aritmética simple.

## 1. Especificaciones Técnicas
* **Memoria de programa:** 7200 bytes (aprox. 7 KB).
* **Variables:** 26 variables globales (A-Z), tipo entero de 16 bits.
* **Aritmética:** Soporta operadores estándar y precedencia básica.
* **Hardware:** Acceso mapeado a LEDs y memoria del sistema.

---

## 2. Comandos de Sistema

| Comando | Descripción |
| :--- | :--- |
| `LIST` | Muestra todas las líneas del programa guardado en memoria. |
| `RUN` | Inicia la ejecución del programa desde la primera línea. |
| `NEW` | Borra el programa actual de la memoria. |
| `FREE` | Indica cuántos bytes quedan disponibles para código BASIC. |
| `QUIT` | Sale del intérprete y regresa al monitor/ROM ($B000). |

---

## 3. Comandos de Programación (Statements)

### `PRINT`
Muestra texto o valores en la terminal.
* **Sintaxis:** `PRINT "TEXTO"` o `PRINT expresión`
* **Ejemplo:** `10 PRINT "EL VALOR DE A ES: ": PRINT A`

### `LET` (Opcional)
Asigna un valor a una variable. El uso de la palabra `LET` es opcional.
* **Sintaxis:** `LET X = 10` o `X = 10`

### `INPUT`
Solicita un número al usuario por teclado.
* **Sintaxis:** `INPUT Variable`
* **Ejemplo:** `20 INPUT B`

### `GOTO`
Salta incondicionalmente a una línea específica.
* **Sintaxis:** `GOTO número_de_línea`

### `IF...THEN`
Ejecución condicional. Soporta comparadores `=`, `==`, `>`, `<`.
* **Sintaxis:** `IF expresión operador expresión THEN comando`
* **Ejemplo:** `30 IF A > 10 THEN GOTO 100`

### `WAIT`
Pausa la ejecución por un tiempo determinado (basado en el reloj del sistema).
* **Sintaxis:** `WAIT milisegundos`
* **Ejemplo:** `40 WAIT 500` (Pausa de medio segundo).

---

## 4. Comandos de Hardware y Memoria

### `LEDS`
Controla los 8 LEDs de la placa Nexys.
* **Sintaxis:** `LEDS valor` (donde valor es 0-255).
* **Ejemplo:** `LEDS 255` (Enciende todos), `LEDS 0` (Apaga todos).

### `POKE`
Escribe un byte en una dirección de memoria específica.
* **Sintaxis:** `POKE dirección, valor`
* **Nota:** Soporta direcciones de 16 bits (0-65535).
* **Ejemplo:** `POKE 49153, 1` (Enciende el LED 0 mediante dirección de memoria $C001).

### `PEEK()`
**Función estricta.** Lee un byte de la memoria. Requiere paréntesis obligatorios.
* **Sintaxis:** `Variable = PEEK(dirección)`
* **Ejemplo:** `50 A = PEEK(49153)`

### `GET`
Lee un carácter del teclado sin esperar a que se presione Enter. Si no hay tecla, devuelve 0.
* **Sintaxis:** `GET Variable`

---

## 5. Operadores Soportados
* **Aritméticos:** `+`, `-`, `*`, `/`, `%` (Módulo/Resto).
* **Lógicos:** `=` (Igual), `==` (Igual), `>` (Mayor que), `<` (Menor que).

---

## 6. Ejemplo de Programa: Contador de LEDs
Este programa incrementa un contador y lo muestra en los LEDs cada 250ms.

```basic
10 LET A = 0
20 LEDS A
30 PRINT "VALOR ACTUAL: "
40 PRINT A
50 A = A + 1
60 IF A == 256 THEN A = 0
70 WAIT 250
80 GOTO 20