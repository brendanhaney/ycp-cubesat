#!/usr/bin/env python3
"""Show UART gyroscope rates as an orthographic, rotating wireframe cube.

Install: python3 -m pip install pyserial pygame
Run:     python3 scripts/view_imu.py [--port /dev/ttyACM0]
Preview: python3 scripts/view_imu.py --demo

Startup is zero rotation. Rates are already zeroed, in body-frame deg/s;
they are integrated into an orientation relative to startup, not compass north.
"""

import argparse
from dataclasses import dataclass
import math
import os
import re
import sys
import time

if __package__:
    from .listen_uart import choose_port, describe_port, open_port
else:
    from listen_uart import choose_port, describe_port, open_port


NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"
GYROSCOPE = re.compile(
    rf"Gyroscope:\s*X=({NUMBER})\s+Y=({NUMBER})\s+Z=({NUMBER})\s+deg/s"
)
IDENTITY = (1.0, 0.0, 0.0, 0.0)
AXES = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
AXIS_COLORS = ((255, 111, 126), (100, 221, 164), (105, 173, 255))
BACKGROUND = (16, 22, 32)
TEXT = (226, 234, 245)
MUTED = (143, 160, 182)
VERTICES = tuple((x, y, z) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1))
EDGES = tuple((a, b) for a in range(8) for b in range(a + 1, 8)
              if sum(x != y for x, y in zip(VERTICES[a], VERTICES[b])) == 1)


def parse_gyroscope(line):
    """Ignore other sensors, partial lines, invalid values and diagnostics."""
    match = GYROSCOPE.fullmatch(line.strip())
    if match is None:
        return None
    rates = tuple(float(value) for value in match.groups())
    return rates if all(math.isfinite(value) for value in rates) else None


def multiply(q, r):
    w, x, y, z = q
    a, b, c, d = r
    return (w*a - x*b - y*c - z*d,
            w*b + x*a + y*d - z*c,
            w*c - x*d + y*a + z*b,
            w*d + x*c - y*b + z*a)


def integrate(orientation, rates, seconds):
    """Right-multiply a body-frame rotation; avoid Euler-angle singularities."""
    if seconds <= 0:
        return orientation
    angular_velocity = tuple(math.radians(rate) for rate in rates)
    speed = math.hypot(*angular_velocity)
    if speed == 0:
        return orientation
    half_angle = speed * seconds / 2
    scale = math.sin(half_angle) / speed
    delta = (math.cos(half_angle), *(value * scale for value in angular_velocity))
    result = multiply(orientation, delta)
    norm = math.hypot(*result)
    return tuple(value / norm for value in result)


def rotate(orientation, point):
    conjugate = (orientation[0], *(-value for value in orientation[1:]))
    return multiply(multiply(orientation, (0.0, *point)), conjugate)[1:]


@dataclass
class Motion:
    orientation: tuple = IDENTITY
    rates: tuple = (0.0, 0.0, 0.0)
    received_at: float = None
    updated_at: float = None
    paused: bool = False
    stale_timeout: float = 3.0

    def advance(self, now):
        if self.updated_at is not None and self.received_at is not None and not self.paused:
            # Hold the last rate between UART samples; freeze on a lost stream.
            end = min(now, self.received_at + self.stale_timeout)
            start = max(self.updated_at, self.received_at)
            self.orientation = integrate(self.orientation, self.rates, end - start)
        self.updated_at = now

    def receive(self, rates, now):
        self.advance(now)
        self.rates = rates
        self.received_at = now

    def reset(self, now):
        self.orientation = IDENTITY
        self.updated_at = now


def read_rates(connection, buffer):
    """Nonblocking UART reads, retaining lines split across reads/frames."""
    waiting = connection.in_waiting
    if not waiting:
        return []
    buffer.extend(connection.read(min(waiting, 4096)))
    lines = buffer.split(b"\n")
    buffer[:] = lines.pop()
    # Bound storage if a disconnected/misconfigured source never sends LF.
    if len(buffer) > 4096:
        buffer.clear()
    samples = []
    for line in lines:
        sample = parse_gyroscope(line.decode("ascii", errors="replace"))
        if sample is not None:
            samples.append(sample)
    return samples


def camera(point):
    """Fixed orthonormal camera: +Z is up, all three axes remain visible."""
    x, y, z = point
    yaw, elevation = math.radians(35), math.radians(25)
    horizontal = math.cos(yaw)*x - math.sin(yaw)*y
    depth = math.sin(yaw)*x + math.cos(yaw)*y
    return (horizontal, math.sin(elevation)*depth - math.cos(elevation)*z,
            math.cos(elevation)*depth + math.sin(elevation)*z)


def draw_scene(pygame, screen, fonts, motion, source, now, error=None):
    screen.fill(BACKGROUND)
    width, height = screen.get_size()
    origin = (width // 2, height // 2 + 30)
    scale = min(width / 7, max(50, (height - 260) / 4.6))
    small, normal, title = fonts

    def project(point):
        x, y, _ = camera(point)
        # Constant scale, with no division by depth: true orthographic view.
        return (round(origin[0] + x * scale), round(origin[1] + y * scale))

    def label(text, position, color=TEXT, font=None, right=False):
        surface = (font or normal).render(text, True, color)
        rect = surface.get_rect(topright=position) if right else surface.get_rect(topleft=position)
        screen.blit(surface, rect)

    label("GYROSCOPE", (26, 22), font=title)
    label("Orthographic / startup = zero", (27, 57), MUTED, small)

    world_color = (68, 82, 102)
    for axis in AXES:
        start, end = project(tuple(-1.9*v for v in axis)), project(tuple(1.9*v for v in axis))
        distance = math.hypot(end[0] - start[0], end[1] - start[1])
        for step in range(0, int(distance), 13):
            a, b = step / distance, min(step + 6, distance) / distance
            pygame.draw.aaline(screen, world_color,
                               (start[0] + (end[0]-start[0])*a, start[1] + (end[1]-start[1])*a),
                               (start[0] + (end[0]-start[0])*b, start[1] + (end[1]-start[1])*b))

    points = [rotate(motion.orientation, vertex) for vertex in VERTICES]
    # Far edges are dimmer, but all twelve stay visible in the wireframe.
    edges = sorted(EDGES, key=lambda edge: sum(camera(points[i])[2] for i in edge))
    for a, b in edges:
        depth = (camera(points[a])[2] + camera(points[b])[2]) / 2
        color = TEXT if depth >= 0 else (98, 117, 142)
        pygame.draw.aaline(screen, color, project(points[a]), project(points[b]))

    for index, (axis, color) in enumerate(zip(AXES, AXIS_COLORS)):
        world_tip = project(tuple(1.95*v for v in axis))
        label("XYZ"[index] + " world", (world_tip[0] + 6, world_tip[1] - 9), color, small)
        body_tip = project(rotate(motion.orientation, tuple(1.35*v for v in axis)))
        pygame.draw.line(screen, color, origin, body_tip, 3)
        dx, dy = body_tip[0] - origin[0], body_tip[1] - origin[1]
        length = math.hypot(dx, dy)
        if length > 1:
            ux, uy = dx / length, dy / length
            pygame.draw.polygon(screen, color, [body_tip,
                                (body_tip[0]-10*ux+4*uy, body_tip[1]-10*uy-4*ux),
                                (body_tip[0]-10*ux-4*uy, body_tip[1]-10*uy+4*ux)])
        label("XYZ"[index] + " body", (body_tip[0] + 7, body_tip[1] + 3), color, small)
    pygame.draw.circle(screen, TEXT, origin, 4)

    panel = pygame.Rect(width - 302, 18, 280, 164)
    pygame.draw.rect(screen, (24, 33, 47), panel, border_radius=10)
    label("UART / RAW RATES", (width - 40, 31), MUTED, small, right=True)
    for index, (rate, color) in enumerate(zip(motion.rates, AXIS_COLORS)):
        value = f'{rate:+10.3f} deg/s' if motion.received_at is not None else '       -- deg/s'
        label(f'{"XYZ"[index]}  {value}', (width - 40, 58 + 27*index), color, right=True)

    age = None if motion.received_at is None else max(0, now - motion.received_at)
    if error:
        status, status_color = "DISCONNECTED / frozen", AXIS_COLORS[0]
    elif age is None:
        status, status_color = "WAITING FOR GYROSCOPE", MUTED
    elif age > motion.stale_timeout:
        status, status_color = f"STALE / last sample {age:.1f}s ago", AXIS_COLORS[0]
    elif motion.paused:
        status, status_color = "PAUSED / receiving data", (244, 199, 114)
    else:
        status, status_color = f"LIVE / sample age {age:.1f}s", AXIS_COLORS[1]
    label(status, (width - 40, 149), status_color, small, right=True)

    label("Dashed: fixed world axes    Arrows: cube body axes", (26, height - 87), MUTED, small)
    label("R  zero orientation     SPACE  pause/resume     ESC  close", (26, height - 61), TEXT, small)
    if error:
        footer = error
    else:
        footer = f"{source}  |  Gyro integration; reference = startup/reset."
    label(footer[:130], (26, height - 33), MUTED, small)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial device, e.g. /dev/ttyACM0 or COM3")
    parser.add_argument("--baud", type=int, default=115200, help="baud rate (default: 115200)")
    parser.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    parser.add_argument("--demo", action="store_true", help="preview rotation without a connected board")
    args = parser.parse_args(argv)
    if args.baud <= 0:
        parser.error("--baud must be positive")

    connection = None
    pygame = None
    try:
        if not args.demo or args.list_ports:
            import serial
            from serial.tools import list_ports
            ports = sorted(list_ports.comports(), key=lambda port: port.device)
            if args.list_ports:
                for port in ports:
                    print(describe_port(port))
                if not ports:
                    print("No serial ports found.")
                return 0
            device = choose_port(ports, args.port)
            connection = open_port(serial, device, args.baud)
            connection.timeout = 0
            connection.reset_input_buffer()
            source = f"{device} / {args.baud} baud / 8N1"
        else:
            source = "DEMO / simulated rates"

        os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")
        import pygame
        pygame.display.init()
        pygame.font.init()
        screen = pygame.display.set_mode((1100, 780), pygame.RESIZABLE)
        pygame.display.set_caption("CubeSat / UART Gyroscope")
        fonts = (pygame.font.SysFont("dejavusans", 14),
                 pygame.font.SysFont("dejavusansmono", 18),
                 pygame.font.SysFont("dejavusans", 25, bold=True))
        motion = Motion()
        buffer = bytearray()
        clock = pygame.time.Clock()
        error = None
        print(f"{source}. Startup = zero rotation. R: reset, SPACE: pause, ESC: close.")

        running = True
        while running:
            now = time.monotonic()
            for event in pygame.event.get():
                if event.type == pygame.QUIT or (event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE):
                    running = False
                elif event.type == pygame.VIDEORESIZE:
                    screen = pygame.display.set_mode((max(800, event.w), max(600, event.h)), pygame.RESIZABLE)
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_r:
                    motion.reset(now)
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_SPACE and error is None:
                    motion.advance(now)
                    motion.paused = not motion.paused

            if args.demo:
                motion.receive((12.0, 22.0, 8.0), now)
            elif error is None:
                try:
                    for rates in read_rates(connection, buffer):
                        motion.receive(rates, now)
                except (serial.SerialException, OSError) as exc:
                    error = f"UART error: {exc}"
                    print(error, file=sys.stderr)
                    motion.paused = True
            motion.advance(now)
            draw_scene(pygame, screen, fonts, motion, source, now, error)
            pygame.display.flip()
            clock.tick(60)
        return 0
    except ImportError as exc:
        print(f"Missing dependency: {exc.name}. Install with: python3 -m pip install pyserial pygame", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"Viewer error: {exc}", file=sys.stderr)
        return 1
    finally:
        if connection is not None:
            connection.close()
        if pygame is not None:
            pygame.quit()


if __name__ == "__main__":
    raise SystemExit(main())
