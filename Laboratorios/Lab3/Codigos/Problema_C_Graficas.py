import serial
import time
import collections
import matplotlib.pyplot as plt

# Parámetros de conexión serial
PORT = "COM8"  # Cambia al puerto adecuado
BAUD = 9600
NPTS = 300  # Número de puntos a mostrar en el gráfico

# Función para extraer los valores de la línea de datos
def get_vals(line):
    try:
        # Se espera que los datos estén en formato: CSV:t(ms),ref_adc,meas_adc,pwm,dir
        # Por ejemplo: "CSV: 1,500,520,180,1"
        parts = line.split(",")  # Separar la línea en componentes por comas
        t_s = int(parts[0].split(":")[1].strip())  # Tiempo (ms)
        ref = int(parts[1].strip())  # Valor del potenciómetro de referencia
        meas = int(parts[2].strip())  # Valor del potenciómetro medido
        pwm = int(parts[3].strip())  # Valor de PWM
        dir = int(parts[4].strip())  # Dirección del motor (0=stop, 1=cw, 2=ccw)
        return t_s, ref, meas, pwm, dir  # Devolver los valores extraídos
    except:
        return None  # Retorna None si hubo un error al procesar la línea

# Configuración de la conexión serial
with serial.Serial(PORT, BAUD, timeout=1) as ser:
    time.sleep(2)  # Tiempo de espera para estabilizar la comunicación
    ser.reset_input_buffer()  # Limpiar el búfer de entrada

    # Leer el encabezado del puerto serial (si se encuentra en el primer mensaje)
    hdr = ser.readline().decode(errors="ignore").strip()  # Leer y limpiar el encabezado
    print(f"Encabezado: {hdr}")  # Mostrar encabezado en consola
    print("-" * 50)  # Línea separadora para mejor visibilidad

    # Colas para almacenar los datos
    xs = collections.deque(maxlen=NPTS)  # Cola para el tiempo
    refq = collections.deque(maxlen=NPTS)  # Cola para el valor de referencia
    measq = collections.deque(maxlen=NPTS)  # Cola para el valor medido
    pwmq = collections.deque(maxlen=NPTS)  # Cola para el valor de PWM
    dirq = collections.deque(maxlen=NPTS)  # Cola para la dirección

    # Configuración de las gráficas
    plt.ion()  # Habilitar modo interactivo para actualización en tiempo real
    fig = plt.figure()  # Crear una nueva figura
    ax1 = plt.gca()  # Obtener el eje actual

    # Líneas de las gráficas para cada parámetro
    ln_ref, = ax1.plot([], [], label="Potenciometro de Referencia", color='blue', linewidth=1)
    ln_meas, = ax1.plot([], [], label="Potenciometro Medido", color='green', linewidth=1)
    ln_pwm, = ax1.plot([], [], label="PWM", color='red', linewidth=1)

    # Configurar los límites de los ejes
    ax1.set_ylim(0, 1023)  # Rango para potenciómetros
    ax1.set_xlabel("Muestras")  # Etiqueta del eje X
    ax1.set_ylabel("Valor")  # Etiqueta del eje Y
    ax1.legend()  # Mostrar leyenda
    ax1.grid(True, alpha=0.3)  # Mostrar la cuadrícula con transparencia
    plt.title("Control de Motor en Vivo")  # Título del gráfico

    # Contador para la lectura de datos
    contador = 0

    while True:
        line = ser.readline().decode(errors="ignore").strip()  # Leer una línea del puerto serial

        if not line:
            continue  # Si la línea está vacía, pasar a la siguiente iteración

        contador += 1
        if contador % 10 == 0:
            print(line)  # Depuración, ver si los datos están llegando correctamente

        if line.startswith("ref"):  # Omitir líneas que no contienen datos válidos
            continue

        vals = get_vals(line)  # Extraer los valores de la línea
        if not vals:
            continue  # Si no se pudieron extraer valores, continuar con la siguiente línea

        t_s, ref, meas, pwm, dir = vals  # Asignar los valores extraídos a variables

        # Almacenar los datos en las colas para las gráficas
        xs.append(len(xs))  # Almacenar el índice de la muestra
        refq.append(ref)  # Almacenar el valor de referencia
        measq.append(meas)  # Almacenar el valor medido
        pwmq.append(pwm)  # Almacenar el valor de PWM

        # Actualizar las gráficas con los nuevos datos
        ln_ref.set_data(range(len(refq)), list(refq))  # Actualizar la línea de referencia
        ln_meas.set_data(range(len(measq)), list(measq))  # Actualizar la línea de medida
        ln_pwm.set_data(range(len(pwmq)), list(pwmq))  # Actualizar la línea de PWM

        # Ajustar los límites de X según el número de muestras
        ax1.set_xlim(0, max(50, len(xs)))  # Mantener un límite mínimo de 50 muestras en el eje X

        # Dibujar las gráficas en tiempo real
        plt.pause(0.001)  # Actualizar las gráficas cada 0.001 segundos

