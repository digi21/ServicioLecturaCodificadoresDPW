#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")

#define DPW_NAME  "\\\\.\\DPWCtrl"
#define DPW_TYPE 50000

#define IOCTL_DPW_GET_DATA \
    CTL_CODE( DPW_TYPE, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS )

#define IOCTL_DPW_RESET_DATA \
    CTL_CODE( DPW_TYPE, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS )

#define NOMBRE_APLICACION L"Servicio de envío de coordenadas de tarjeta DPW por UDP/IP"

struct HandlesData
{
	LONG     X, Y, Z;
	UCHAR    PedalsSwitch;
	UCHAR    Status;
	UCHAR    PedalsState;
	UCHAR    HwVersion;
};

struct CoordenadaDispositivoEntrada
{
	long X;
	long Y;
	long Z;
	BYTE Pedal;
};

namespace {
	wchar_t direccion[256];
	int puerto = 0;
	SERVICE_STATUS serviceStatus{};
	SERVICE_STATUS_HANDLE hStatus{};

	void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
	void WINAPI ServiceCtrlHandler(DWORD ctrlCode);
	void ComunicaErrorEnVisorEventos(const wchar_t* mensaje, DWORD codigoError);
	void WriteToEventLog(const wchar_t* message, DWORD type=EVENTLOG_INFORMATION_TYPE);
}

static int wmain(int argc, wchar_t* argv[])
{
	if (argc < 3) {
		ComunicaErrorEnVisorEventos(L"No se han especificado suficientes parámetros al servicio: %d", argc);
		return 1;
	}

	wcscpy_s(direccion, argv[1]);
	puerto = _wtoi(argv[2]);


	constexpr SERVICE_TABLE_ENTRY serviceTable[] = {
		{ const_cast<LPTSTR>(NOMBRE_APLICACION), static_cast<LPSERVICE_MAIN_FUNCTIONW>(ServiceMain) },
		{ nullptr, nullptr}
	};

	if (!StartServiceCtrlDispatcher(serviceTable))
		return static_cast<int>(GetLastError());

	return 0;
}

namespace {
	void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
	{
		hStatus = RegisterServiceCtrlHandler(NOMBRE_APLICACION, ServiceCtrlHandler);
		if (!hStatus)
			return;

		serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		serviceStatus.dwCurrentState = SERVICE_START_PENDING;
		serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

		SetServiceStatus(hStatus, &serviceStatus);

		// Código de inicialización del servicio
		auto const file = CreateFileA(
			DPW_NAME,
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr
		);

		if (file == INVALID_HANDLE_VALUE)
		{
			ComunicaErrorEnVisorEventos(L"No se pudo conectar con el driver de la tarjeta DPW PCI: %d", GetLastError());
			return;
		}

		WSADATA wsaData;
		if (auto const resultado = WSAStartup(MAKEWORD(2, 2), &wsaData); resultado != 0) 
		{
			ComunicaErrorEnVisorEventos(L"Error en WSAStartup: %d", resultado);
			return;
		}

		SOCKET const udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udpSocket == INVALID_SOCKET) 
		{
			ComunicaErrorEnVisorEventos(L"Error al crear el socket: %x", WSAGetLastError());
			WSACleanup();
			return;
		}

		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(puerto);
		InetPtonW(AF_INET, direccion, &serverAddr.sin_addr);


		// Comunicamos que el servicio está ejecutándose
		serviceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(hStatus, &serviceStatus);

		// Código de ejecución del servicio
		bool pedalBajado[] = { false, false, false, false, false };
		HandlesData datos{};
		HandlesData oldDatos{};

		while (serviceStatus.dwCurrentState == SERVICE_RUNNING) 
		{
			DWORD bytesReturned;
			if (!DeviceIoControl(
				file,
				IOCTL_DPW_GET_DATA,
				nullptr,
				0,
				&datos,
				sizeof(datos),
				&bytesReturned,
				nullptr))
			{
				ComunicaErrorEnVisorEventos(L"Error al leer los datos de la tarjeta DPW PCI: %d", GetLastError());
				break;
			}

			if (oldDatos.X != datos.X || oldDatos.Y != datos.Y || oldDatos.Z != datos.Z || oldDatos.PedalsSwitch != datos.PedalsSwitch) {
				oldDatos = datos;

				auto pedal = datos.PedalsSwitch;

				// Esta tarjeta manda 1,2,4 al pulsar un pedal y despues lo pone a 0
				// Cuando se levanta el pedal manda 1+128 , 2+128, 4+128
				if (pedal) {
					if (pedal < 0x80) {
						pedalBajado[pedal] = true;
					}
					else {
						pedalBajado[pedal - 0x80] = false;
					}
				}

				pedal = 0;
				pedal |= pedalBajado[1] ? 0x1 : 0;
				pedal |= pedalBajado[2] ? 0x2 : 0;
				pedal |= pedalBajado[4] ? 0x4 : 0;

				CoordenadaDispositivoEntrada datosEnviar{};
				datosEnviar.X = datos.X;
				datosEnviar.Y = datos.Y;
				datosEnviar.Z = datos.Z;
				datosEnviar.Pedal = pedal;

				if (sendto(udpSocket, reinterpret_cast<char const*>(&datosEnviar), sizeof(datosEnviar), 0, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) 
				{
					ComunicaErrorEnVisorEventos(L"Error al enviar coordenadas: %x", WSAGetLastError());
					break;
				}
			}

			// Pausamos 1ms para evitar consumir un núcleo completo
			Sleep(1);
		}

		closesocket(udpSocket);
		WSACleanup();
		CloseHandle(file);

		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &serviceStatus);
	}

	void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
		switch (ctrlCode)
		{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(hStatus, &serviceStatus);
			break;
		default:
			break;
		}
	}

	void WriteToEventLog(const wchar_t* message, DWORD type)
	{
		auto const hEventLog = RegisterEventSource(nullptr, NOMBRE_APLICACION);
		if (hEventLog == nullptr) {
			if (wchar_t cadenaError[256]; swprintf_s(cadenaError, L"RegisterEventSource falló con el error: %d", GetLastError()))
			{
				OutputDebugString(cadenaError);
			}
			return;
		}

		const wchar_t* messages[1];
		messages[0] = message;

		if (!ReportEvent(hEventLog, type, 0, 0, nullptr, 1, 0, messages, nullptr))
		{
			if (wchar_t cadenaError[256]; swprintf_s(cadenaError, L"ReportEvent falló con el error: %d", GetLastError()))
			{
				OutputDebugString(cadenaError);
			}
		}

		DeregisterEventSource(hEventLog);
	}

	void ComunicaErrorEnVisorEventos(const wchar_t* mensaje, DWORD codigoError)
	{
		if (wchar_t cadenaError[256]; swprintf_s(cadenaError, mensaje, codigoError))
		{
			WriteToEventLog(cadenaError, EVENTLOG_ERROR_TYPE);
		}
	}
}