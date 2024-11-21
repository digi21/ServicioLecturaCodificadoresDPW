# ServicioLecturaCodificadoresDPW

Este repositorio contiene el c�digo fuente del servicio de lectura de codificadores PCI de la estaci�n fotogram�trica ucraniana Digital Photogrammetric Station "Delta".

Geosystem (ahora Analytica Ltd) nunca desarroll� un driver de 64 bits para su tarjeta PCI de lectura de codificadores DPW, de manera que no se puede instalar el driver en una versi�n de Windows de 64 bits.
La soluci�n al problema consiste en el servicio que se presenta en este repositorio, que se debe ejecutar en un Windows de 32 bits, con el driver de la tarjeta DPW instalado correctamente. 
Este servicio env�a las coordenadas a la m�quina con Digi3D.NET mediante el protocolo UDP/IP.

En noviembre de 2024 se ha a�adido a Digi3D.NET la posibilidad de seleccionar como dispositivo de entrada para la ventana fotogram�trica el dispositivo UDP/IP que recibe el siguiente paquete:

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
sc create "Servicio de lectura de codificadores DPW" binPath= "C:\Ruta\Del\Directorio\Del\ServicioLecturaCodificadoresDPW.exe [direcci�n IP de la m�quina con Digi3D.NET] [puerto]"
sc start "Servicio de lectura de codificadores DPW"
```

Por ejemplo, si la direcci�n IP de la m�quina con Digi3D.NET es 192.168.1.37 y el puerto es 8888, el comando ser�a:

```cmd
sc create "Servicio de lectura de codificadores DPW" binPath= "C:\Ruta\Del\Directorio\Del\ServicioLecturaCodificadoresDPW.exe 192.168.1.37 8888"
sc start "Servicio de lectura de codificadores DPW"
```
