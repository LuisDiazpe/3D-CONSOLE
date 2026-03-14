# 3D-CONSOLE
# LOIKO CONSOLE

> A handmade 3D space shooter running on an Arduino UNO with a 128×64 OLED display.

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Platform: Arduino UNO](https://img.shields.io/badge/Platform-Arduino%20UNO-teal.svg)
![Display: SSD1306 128x64](https://img.shields.io/badge/Display-SSD1306%20128x64-orange.svg)

---

## What is this?

LOIKO CONSOLE is a fully self-contained retro game console built with an Arduino UNO, a joystick module, and a tiny OLED screen. Everything — the 3D engine, the boot animation, the game, the menu — runs on an 8-bit AVR microcontroller with 32 KB of flash and 2 KB of RAM.

The game is a first-person 3D space shooter: asteroids fly toward you, you dodge and shoot them down. The score is saved to EEPROM so it survives power cycles.

### Features

- **3D wormhole boot animation** — a wireframe sphere rotates in true 3D (dual-axis rotation), explodes into a portal with expanding rings and radial rays, then the LOIKO CONSOLE logo emerges letter by letter. Runs for ~12 seconds on startup.
- **3D perspective projection** — all objects (ship, asteroids, starfield) are rendered with real perspective using integer arithmetic, no floating-point in the boot path.
- **First-person space shooter** — fly through an asteroid field, auto-fire lasers, survive as long as possible.
- **Animated main menu** — cinematic 3D demo plays in the top half while you navigate options.
- **Pause menu** — pause mid-game, resume or return to the main menu cleanly.
- **EEPROM high score** — your best score is saved between sessions.
- **Fits on an Arduino UNO** — ~78% flash, ~43% SRAM after all optimizations.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | Arduino UNO (ATmega328P) |
| Display | SSD1306 OLED 128×64, I²C |
| Input | Analog joystick module (KY-023 or similar) |

### Wiring

```
OLED   VCC → 5V     GND → GND     SDA → A4     SCL → A5
JOY    +5V → 5V     GND → GND     VRx → A0     VRy → A1     SW → D2
```

---

## Software dependencies

Install these via the Arduino Library Manager before compiling:

- **Adafruit SSD1306** ≥ 2.5
- **Adafruit GFX Library** ≥ 1.11

---

## How to build

1. Clone this repository.
2. Open `SC3D/SC3D.ino` in the Arduino IDE.
3. Install the libraries listed above.
4. Select **Arduino UNO** as the board.
5. Upload.

---

## How to play

| Action | Control |
|---|---|
| Move ship | Joystick (any direction) |
| Shoot | Automatic |
| Pause / Resume | Press joystick button |
| Navigate menus | Joystick up / down |
| Confirm | Press joystick button |

Asteroids spawn from the distance and drift toward you. Each one destroyed scores **10 points**. You have **3 lives** — an asteroid reaching you or colliding with your ship costs one life.

---

## How the 3D works

Everything is built on two operations applied to every object every frame:

**1. Rotation** — each vertex `(x, y, z)` is multiplied by a rotation matrix using `sin`/`cos`. The boot animation uses a 64-entry integer sine table in PROGMEM to avoid pulling in the AVR float library.

**2. Perspective projection** — a rotated point is mapped to a screen pixel with:
```
sx = CX + x * FOV / z
sy = CY - y * FOV / z
```
Dividing by `z` makes far objects smaller — that is the entire 3D illusion.

The sphere in the boot animation has no stored vertices. Its points are computed on the fly using the circle formula `(cos(a)·r, sin(a)·r)` for each latitude ring and meridian, then rotated and projected like everything else.

---

## Project structure

```
SC3D/
└── SC3D.ino    — full source, single file
README.md
LICENSE
```

---

## License

This project is licensed under the **GNU General Public License v3.0**.  
See [LICENSE](LICENSE) for the full text.

You are free to use, study, modify and distribute this project, provided that any derivative work is also released under the GPL v3.

---
---

# LOIKO CONSOLE — Español

> Un shooter espacial 3D hecho a mano, corriendo en un Arduino UNO con pantalla OLED 128×64.

---

## ¿Qué es esto?

LOIKO CONSOLE es una consola de videojuego retro construida con un Arduino UNO, un módulo de joystick y una pequeña pantalla OLED. Todo — el motor 3D, la animación de arranque, el juego, el menú — corre en un microcontrolador AVR de 8 bits con 32 KB de flash y 2 KB de RAM.

El juego es un shooter espacial en primera persona: los asteroides vuelan hacia ti, los esquivas y los destruyes. El puntaje se guarda en la EEPROM y sobrevive los cortes de corriente.

### Características

- **Animación de boot wormhole en 3D** — una esfera wireframe rota en 3D real (rotación de doble eje), explota en un portal con anillos expansivos y rayos radiales, luego el logo LOIKO CONSOLE emerge letra por letra. Dura ~12 segundos al encender.
- **Proyección en perspectiva 3D** — todos los objetos (nave, asteroides, campo de estrellas) se renderizan con perspectiva real usando aritmética entera.
- **Shooter espacial en primera persona** — vuela entre un campo de asteroides, dispara lasers automáticamente, sobrevive lo más posible.
- **Menú principal animado** — una demo cinemática 3D se reproduce en la mitad superior mientras navegas las opciones.
- **Menú de pausa** — pausa en mitad del juego, continúa o vuelve al menú limpiamente.
- **High score en EEPROM** — tu mejor puntaje se guarda entre sesiones.
- **Cabe en un Arduino UNO** — ~78% de flash, ~43% de SRAM después de todas las optimizaciones.

---

## Hardware

| Componente | Detalles |
|---|---|
| Microcontrolador | Arduino UNO (ATmega328P) |
| Pantalla | SSD1306 OLED 128×64, I²C |
| Entrada | Módulo joystick analógico (KY-023 o similar) |

### Conexiones

```
OLED   VCC → 5V     GND → GND     SDA → A4     SCL → A5
JOY    +5V → 5V     GND → GND     VRx → A0     VRy → A1     SW → D2
```

---

## Dependencias de software

Instálalas desde el Gestor de Librerías del Arduino IDE antes de compilar:

- **Adafruit SSD1306** ≥ 2.5
- **Adafruit GFX Library** ≥ 1.11

---

## Cómo compilar y subir

1. Clona este repositorio.
2. Abre `SC3D/SC3D.ino` en el Arduino IDE.
3. Instala las librerías indicadas arriba.
4. Selecciona **Arduino UNO** como placa.
5. Sube el sketch.

---

## Cómo jugar

| Acción | Control |
|---|---|
| Mover la nave | Joystick (cualquier dirección) |
| Disparar | Automático |
| Pausar / Reanudar | Pulsar el botón del joystick |
| Navegar menús | Joystick arriba / abajo |
| Confirmar | Pulsar el botón del joystick |

Los asteroides aparecen en la distancia y se acercan hacia ti. Cada uno destruido suma **10 puntos**. Tienes **3 vidas** — un asteroide que llega hasta ti o que colisiona con tu nave cuesta una vida.

---

## Cómo funciona el 3D

Todo se basa en dos operaciones que se aplican a cada objeto en cada frame:

**1. Rotación** — cada vértice `(x, y, z)` se multiplica por una matriz de rotación usando `sin`/`cos`. La animación de boot usa una tabla de seno de 64 entradas en PROGMEM para no arrastrar la librería de punto flotante de AVR.

**2. Proyección en perspectiva** — un punto rotado se convierte en un píxel de pantalla con:
```
sx = CX + x * FOV / z
sy = CY - y * FOV / z
```
Dividir por `z` hace que los objetos lejanos se vean más pequeños — esa es toda la ilusión del 3D.

La esfera de la animación de boot no tiene vértices guardados. Sus puntos se calculan al vuelo usando la fórmula del círculo `(cos(a)·r, sin(a)·r)` para cada anillo de latitud y meridiano, luego se rotan y proyectan igual que todo lo demás.

---

## Licencia

Este proyecto está licenciado bajo la **GNU General Public License v3.0**.  
Consulta el archivo [LICENSE](LICENSE) para el texto completo.

Eres libre de usar, estudiar, modificar y distribuir este proyecto, siempre que cualquier trabajo derivado también se publique bajo la GPL v3.