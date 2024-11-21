# ServicioLecturaCodificadoresDPW

Este repositorio contiene el código fuente del servicio de lectura de codificadores PCI de la estación fotogramétrica ucraniana Digital Photogrammetric Station "Delta".

Geosystem (ahora Analytica Ltd) nunca desarrolló un driver de 64 bits para su tarjeta PCI de lectura de codificadores DPW, de manera que no se puede instalar el driver en una versión de Windows de 64 bits.
La solución al problema consiste en el servicio que se presenta en este repositorio, que se debe ejecutar en un Windows de 32 bits, con el driver de la tarjeta DPW instalado correctamente. 
Este servicio envía las coordenadas a la máquina con Digi3D.NET mediante el protocolo UDP/IP.

En noviembre de 2024 se ha añadido a Digi3D.NET la posibilidad de seleccionar como dispositivo de entrada para la ventana fotogramétrica el dispositivo UDP/IP que recibe el siguiente paquete:

```cpp
struct COORDENADA_DISPOSITIVO_ENTRADA
{
	long X;
	long Y;
	long Z;
	BYTE Pedal;
};
```

Para instalar el servicio tenemos que ejecutar en una consola de comandos con permisos de administrador el siguiente comando:

```cmd
sc create "Servicio de lectura de codificadores DPW" binPath= "C:\Ruta\Del\Directorio\Del\ServicioLecturaCodificadoresDPW.exe [dirección IP de la máquina con Digi3D.NET] [puerto]"
sc start "Servicio de lectura de codificadores DPW"
```

Por ejemplo, si la dirección IP de la máquina con Digi3D.NET es 192.168.1.37 y el puerto es 8888, el comando sería:

```cmd
sc create "Servicio de lectura de codificadores DPW" binPath= "C:\Ruta\Del\Directorio\Del\ServicioLecturaCodificadoresDPW.exe 192.168.1.37 8888"
sc start "Servicio de lectura de codificadores DPW"
```
