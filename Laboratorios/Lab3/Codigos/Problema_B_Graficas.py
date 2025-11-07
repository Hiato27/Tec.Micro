# Librerías
import matplotlib
matplotlib.use("TkAgg") 
import argparse
import csv
import re
import sys
import time
from pathlib import Path
from collections import deque
import threading  
import serial  # pyserial para lectura del puerto COM
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# Expresión regular para extraer datos en formato CSV
CSV_RE = re.compile(r"CSV:\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*,\s*(\d+)", re.IGNORECASE)
PM_OK_RE = re.compile(r"OK\s+(?:punto_medio|pm)\s*=\s*(\d+)", re.IGNORECASE)


g_running = True  # Bandera para detener ambos hilos



#Escucha comandos de la terminal
def command_listener(serial_port_obj):
    """
    Esta función se ejecuta en un hilo separado.
    Su único trabajo es esperar un input() y enviarlo al puerto serie.
    """
    global g_running
    print("\n[INFO] Hilo de comandos iniciado. Escribe 'q' para salir.")
    print("[INFO] Escribe comandos (ej. 'h' o 'm') y presiona Enter.")

    while g_running:
        try:
            command = input()  # Espera aquí (ej. el usuario escribe 'm' y da Enter)

            if not g_running:
                break

            if command.lower() == 'q':
                print("[INFO] Comando 'q' recibido. Deteniendo...")
                g_running = False  # Avisa al hilo principal
                break

            # Si no es para salir, enviamos el comando al Arduino
            if serial_port_obj and serial_port_obj.is_open:

                # Enviamos SÓLO el comando (ej. 'm')
                serial_port_obj.write(command.encode('utf-8'))

                # Esperamos 50ms. Esto le da tiempo al Arduino
                time.sleep(0.05)

                # Enviamos el 'Enter' (newline) por separado.
                serial_port_obj.write(b'\n')

            else:
                if g_running:
                    print("[WARN] Puerto serie no disponible. No se envió comando.")

        except (EOFError, KeyboardInterrupt):
            if g_running:
                print("[INFO] Hilo de comandos detenido por excepción.")
                g_running = False
        except Exception as e:
            if g_running:
                print(f"[ERROR] Hilo de comandos: {e}")
                g_running = False


Hilo pricipal con parametros
def parse_args():
    p = argparse.ArgumentParser("Monitor en vivo (1 ventana, 3 gráficas) + Logger CSV")
    p.add_argument("--port", required=True, help="COMx / /dev/ttyACM0 / /dev/ttyUSB0")
    p.add_argument("--baud", type=int, default=9600, help="Baudios (default 9600)")
    p.add_argument("--outfile", default="log_control.csv", help="Ruta CSV de salida")
    p.add_argument("--pm", type=float, default=26.0, help="Punto medio inicial (°C)")
    p.add_argument("--maxpoints", type=int, default=1200, help="Puntos máximos en memoria")
    p.add_argument("--interval", type=float, default=1.0, help="Refresco de gráficos (s)")
    p.add_argument("--debug", action="store_true", help="Imprimir líneas crudas recibidas")
    return p.parse_args()


# Main
def main():
    global g_running
    args = parse_args()

    # Buffers circulares para almacenar las lecturas
    t_buf = deque(maxlen=args.maxpoints)
    T_buf = deque(maxlen=args.maxpoints)
    H_buf = deque(maxlen=args.maxpoints)
    F_buf = deque(maxlen=args.maxpoints)
    pm_val = float(args.pm)

    # CSV de salida
    csv_path = Path(args.outfile)
    csv_path.parent.mkdir(parents=True, exist_ok=True)  # Crear directorio si no existe
    fcsv = open(csv_path, "w", newline="")
    writer = csv.writer(fcsv)
    writer.writerow(["t_s", "temp_c", "heater_pwm", "fan_pwm", "pm_used"])

    # Abrir puerto serial
    try:
        ser = serial.Serial(args.port, baudrate=args.baud, timeout=0.3)
        print("[INFO] Puerto serie abierto. Esperando 2 segundos a que el Arduino se reinicie...")
        time.sleep(2.0)
        print("[INFO] Listo.")
    except Exception as e:
        print(f"[ERROR] No se pudo abrir {args.port}: {e}")
        return 1

    # iniciar hilos de comando
    input_thread = threading.Thread(
        target=command_listener,
        args=(ser,),
        daemon=True
    )
    input_thread.start()

    # Configuración de Gráfica 
    plt.ion()  # Habilitar modo interactivo para actualización en tiempo real
    fig = plt.figure("Control de Temperatura (en vivo)", figsize=(12, 7))
    gs = GridSpec(2, 2, figure=fig, height_ratios=[2.0, 1.0], hspace=0.35, wspace=0.25)

    # Temperatura
    axT = fig.add_subplot(gs[0, :])
    # Heater
    axH = fig.add_subplot(gs[1, 0])
    # Fan
    axF = fig.add_subplot(gs[1, 1])

    # Configuración de los ejes para la temperatura
    axT.set_title("Temperatura vs Tiempo (con banda ideal pm±3)")
    axT.set_xlabel("Tiempo (s)")
    axT.set_ylabel("Temperatura (°C)")
    axT.grid(True)
    lineT, = axT.plot([], [], label="Temperatura (°C)")
    band_poly = None
    axT.legend()

    # Configuración de los ejes para PWM del calefactor
    axH.set_title("Heater (PWM)")
    axH.set_xlabel("Tiempo (s)")
    axH.set_ylabel("PWM (0-255)")
    axH.set_ylim(-5, 260)
    axH.grid(True)
    lineH, = axH.plot([], [], label="Heater PWM")
    axH.legend()

    # Configuración de los ejes para PWM del ventilador
    axF.set_title("Ventilador (PWM)")
    axF.set_xlabel("Tiempo (s)")
    axF.set_ylabel("PWM (0-255)")
    axF.set_ylim(-5, 260)
    axF.grid(True)
    lineF, = axF.plot([], [], label="Fan PWM")
    # Marcas de referencia visuales para los niveles de PWM
    for y, txt in [(0, "OFF"), (100, "LOW"), (170, "MED"), (240, "HIGH")]:
        axF.axhline(y, linestyle="--")
        axF.text(0.01, (y + 5) / 260.0, txt, transform=axF.transAxes, va="bottom")
    axF.legend()

    fig.canvas.draw()
    plt.show(block=False)

    print(f"[INFO] Conectado a {args.port} @ {args.baud}. CSV -> {csv_path.resolve()}")
    last_plot = 0.0
    last_data = time.time()

    # Bucle principal
    try:
        while g_running:
            # Lee el serial
            raw = ser.readline()
            if raw:
                # Decodificar a str
                try:
                    line = raw.decode(errors="ignore").strip()
                except Exception:
                    line = ""

                if not line:  # Ignora líneas vacías
                    continue

                if args.debug:
                    print(f"[DBG] {line}")

                # Busca si es una línea CSV
                m = CSV_RE.search(line)
                # Busca si es una línea de OK
                mpm = PM_OK_RE.search(line)

                if m:
                    # SÍ ES CSV guarda datos
                    try:
                        t_s = int(m.group(1))
                        tempC = int(m.group(2))
                        hpwm = int(m.group(3))
                        fpwm = int(m.group(4))
                    except Exception:
                        t_s = None

                    if t_s is not None:
                        last_data = time.time()
                        t_buf.append(t_s)
                        T_buf.append(tempC)
                        H_buf.append(hpwm)
                        F_buf.append(fpwm)
                        writer.writerow([t_s, tempC, hpwm, fpwm, pm_val])
                        fcsv.flush()
                        # Imprime el log de datos
                        print(f"t={t_s:>4}s  T={tempC:>3}°C  H={hpwm:>3}  F={fpwm:>3}  pm={pm_val:.1f}")

                elif mpm:
                    # SÍ ES 'OK PM': Actualiza el valor de pm_val
                    try:
                        pm_val = float(mpm.group(1))
                        # Imprime el log de info
                        print(f"[INFO] pm actualizado por firmware: {pm_val:.1f} °C")
                    except Exception:
                        pass

                else:
                    # Imprimir log
                    print(f"[Arduino]: {line}")

            else:
                # No se recibió 'raw' (timeout)
                if time.time() - last_data > 5.0:
                    print("[WARN] 5 s sin datos. ¿Puerto correcto? ¿Otro programa abrió el COM?")
                    last_data = time.time()

            # Actualización de gráficos 
            now = time.time()
            if now - last_plot >= args.interval:
                if len(t_buf) >= 2:
                    lineT.set_data(list(t_buf), list(T_buf))
                    axT.relim();
                    axT.autoscale_view()
                    if band_poly is not None:
                        band_poly.remove()
                        band_poly = None
                    lo = pm_val - 3.0;
                    hi = pm_val + 3.0
                    band_poly = axT.fill_between(list(t_buf), lo, hi, alpha=0.2, label="Banda ideal (pm±3)")
                    handles, labels = axT.get_legend_handles_labels()
                    seen = set();
                    H2 = [];
                    L2 = []
                    for h, l in zip(handles, labels):
                        if l not in seen:
                            H2.append(h);
                            L2.append(l);
                            seen.add(l)
                    axT.legend(H2, L2)
                    lineH.set_data(list(t_buf), list(H_buf))
                    axH.relim();
                    axH.autoscale_view()
                    lineF.set_data(list(t_buf), list(F_buf))
                    axF.relim();
                    axF.autoscale_view()
                    try:
                        fig.canvas.draw_idle()
                        plt.pause(0.001)
                    except Exception as e:
                        if "application has been destroyed" in str(e) or "TclError" in str(e):
                            print("[INFO] Ventana de gráfica cerrada. Saliendo...")
                            g_running = False
                        else:
                            raise e
                last_plot = now

    # Salida con Ctrl+C
    except KeyboardInterrupt:
        print("\n[INFO] Saliendo (Ctrl+C)...")
        g_running = False
    finally:
        g_running = False
        print("[INFO] Limpiando y cerrando...")
        try:
            ser.close()
        except Exception:
            pass
        try:
            fcsv.close()
        except Exception:
            pass
        plt.ioff()
        print("[INFO] Hecho.")

    return 0


# Punto de entrada
if __name__ == "__main__":
    sys.exit(main())

