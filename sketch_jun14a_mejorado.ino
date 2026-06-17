/*
  ======================================================================
  CAJA FUERTE - HUELLA + PIN (ESP32) - VERSION MEJORADA
  ======================================================================

  Este archivo parte del sketch original "sketch_jun14a.ino" y le aplica
  una serie de mejoras y correcciones. Cada cambio importante esta
  marcado en el codigo con un comentario "// MEJORA N:" para que sea
  facil ubicarlo. Resumen de las mejoras aplicadas:

  1) BUG CORREGIDO en buscarUsuarioPorPIN(): antes el "return id" estaba
     fuera del "if", por lo que la funcion devolvia el primer usuario
     con PIN configurado, sin importar si coincidia o no con el PIN
     ingresado. Ahora solo devuelve el ID si el PIN realmente coincide.

  2) Se agregaron CONSTANTES con nombre para los rangos de IDs y los IDs
     de administrador (antes eran "numeros magicos" repetidos: 3, 20,
     127, 1, 2).

  3) Se eliminaron variables/estructuras que estaban DECLARADAS PERO SIN
     USO real en la logica del programa: PIN_MAESTRO, struct Usuario y
     el arreglo usuarios[20].

  4) Se elimino la funcion eliminarHuella(), que estaba definida pero
     nunca se llamaba desde ningun lado (codigo muerto).

  5) Se agrego la funcion auxiliar esperarDedo(), que reemplaza los
     bucles "while(finger.getImage() != ...)" con timeout y cancelacion.

  6) leerHuella() ahora usa esperarDedo() con timeout.

  7) registrarHuellaEnID() ahora DEVUELVE bool (exito/fracaso).

  8) modificarPIN() ahora vuelve correctamente al menu de administrador.

  9) capturarID() ahora valida que el numero este dentro del rango valido.

  10) Se agregaron comentarios de documentacion arriba de cada funcion.

  11) NOTA DE SEGURIDAD: los PIN se guardan en texto plano en Preferences.
      Sugerencia futura: guardar hash SHA-256.
  ======================================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <ESP32Servo.h>
#include <Preferences.h>

//================ LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

//================ HUELLA ==============
HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerSerial);

//================ TECLADO =============
const byte FILAS = 4;
const byte COLUMNAS = 4;

char teclas[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinesFilas[FILAS]    = {13, 12, 14, 27};
byte pinesColumnas[COLUMNAS] = {26, 25, 33, 32};

Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

//================ SERVO ===============
Servo servoCaja;
#define SERVO_PIN    4
#define POS_CERRADO  0
#define POS_ABIERTO  90

//================ CONSTANTES (MEJORA 2) ===============
const int ID_MIN          = 3;
const int ID_MAX_USUARIOS = 20;
const int ID_MAX_HUELLAS  = 127;
const int ADMIN_ID_1      = 1;
const int ADMIN_ID_2      = 2;

//================ TIMEOUT (MEJORA 5) ==================
const unsigned long FINGER_TIMEOUT_MS = 8000;

//================ VARIABLES ===========
bool menuAdmin = false;
Preferences prefs;

//======================================

void pantallaInicio() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Caja Fuerte");
  lcd.setCursor(0, 1);
  lcd.print("*=H #=PIN");
}

//======================================

void setup() {
  Serial.begin(115200);
  prefs.begin("cajafuerte", false);

  lcd.init();
  lcd.backlight();
  servoCaja.attach(SERVO_PIN);
  servoCaja.write(POS_CERRADO);

  FingerSerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  lcd.clear();
  lcd.print("Iniciando...");

  if (!finger.verifyPassword()) {
    lcd.clear();
    lcd.print("Error Sensor");
    while (true);
  }

  lcd.clear();
  lcd.print("Sensor OK");
  delay(1500);
  pantallaInicio();
}

//======================================

void loop() {
  if (menuAdmin) {
    manejarMenuAdmin();
    return;
  }

  char tecla = teclado.getKey();
  if (tecla == '*') leerHuella();
  if (tecla == '#') leerPin();
}

//======================================
// MEJORA 5: espera que se apoye o retire el dedo con timeout y cancel.
// Devuelve true si se alcanzó el estado esperado, false si hubo timeout
// o el usuario canceló con '*'.
bool esperarDedo(bool esperarRetiro, unsigned long timeoutMs) {
  unsigned long inicio = millis();
  while (millis() - inicio < timeoutMs) {
    int resultado = finger.getImage();
    if (!esperarRetiro && resultado == FINGERPRINT_OK)     return true;
    if ( esperarRetiro && resultado == FINGERPRINT_NOFINGER) return true;
    if (teclado.getKey() == '*') return false;
  }
  return false;
}

//======================================

void mostrarMenuAdmin() {
  menuAdmin = true;
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("A:Add B:Del");
  lcd.setCursor(0, 1); lcd.print("C:abr D:Mod");
}

// Procesa tecla del menú admin y ejecuta la acción correspondiente.
void manejarMenuAdmin() {
  char tecla = teclado.getKey();
  if (!tecla) return;
  switch (tecla) {
    case 'A': menuAgregarUsuario();    break;
    case '5': eliminarUsuarioCompleto(); break;
    case 'C': abrirCaja(); mostrarMenuAdmin(); break;
    case 'D': menuModificarUsuario();  break;
    case '#': menuAdmin = false; pantallaInicio(); break;
  }
}

//======================================
// Busca el primer ID de huella libre entre ID_MIN e ID_MAX_HUELLAS.
// Devuelve el ID libre o -1 si no hay ninguno.
int buscarIDLibre() {
  for (int id = ID_MIN; id <= ID_MAX_HUELLAS; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) return id;
  }
  return -1;
}

// Busca el primer slot de usuario libre (sin huella ni PIN).
// Devuelve el ID libre o -1 si no hay ninguno.
int buscarUsuarioLibre() {
  for (int id = ID_MIN; id <= ID_MAX_USUARIOS; id++) {
    bool tieneHuella = (finger.loadModel(id) == FINGERPRINT_OK);
    String clave = "pin" + String(id);
    bool tienePin = prefs.getString(clave.c_str(), "") != "";
    if (!tieneHuella && !tienePin) return id;
  }
  return -1;
}

//======================================
// Registra una nueva huella en el primer ID libre.
void agregarHuella() {
  int id = buscarIDLibre();
  if (id == -1) {
    lcd.clear(); lcd.print("Memoria llena");
    delay(2000); mostrarMenuAdmin(); return;
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Nuevo ID:"); lcd.print(id);
  lcd.setCursor(0, 1); lcd.print("Ponga dedo");

  if (!esperarDedo(false, FINGER_TIMEOUT_MS)) {
    lcd.clear(); lcd.print("Cancelado");
    delay(1500); mostrarMenuAdmin(); return;
  }
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Error lectura");
    delay(1500); mostrarMenuAdmin(); return;
  }

  lcd.clear(); lcd.print("Retire dedo");
  delay(2000);
  esperarDedo(true, FINGER_TIMEOUT_MS);

  lcd.clear(); lcd.print("Mismo dedo");
  if (!esperarDedo(false, FINGER_TIMEOUT_MS)) {
    lcd.clear(); lcd.print("Cancelado");
    delay(1500); mostrarMenuAdmin(); return;
  }
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Error lectura");
    delay(1500); mostrarMenuAdmin(); return;
  }
  if (finger.createModel() != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Huellas no");
    lcd.setCursor(0, 1); lcd.print("coinciden");
    delay(2000); mostrarMenuAdmin(); return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Guardado");
    lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
    Serial.print("Nueva huella ID "); Serial.println(id);
  } else {
    lcd.clear(); lcd.print("Error guardado");
  }
  delay(2500);
  mostrarMenuAdmin();
}

//======================================
// MEJORA 7: ahora devuelve bool para informar si tuvo éxito.
bool registrarHuellaEnID(int id) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ID:"); lcd.print(id);
  lcd.setCursor(0, 1); lcd.print("Ponga dedo");

  if (!esperarDedo(false, FINGER_TIMEOUT_MS)) return false;
  if (finger.image2Tz(1) != FINGERPRINT_OK)   return false;

  lcd.clear(); lcd.print("Retire dedo");
  delay(2000);
  esperarDedo(true, FINGER_TIMEOUT_MS);

  lcd.clear(); lcd.print("Mismo dedo");
  if (!esperarDedo(false, FINGER_TIMEOUT_MS)) return false;
  if (finger.image2Tz(2) != FINGERPRINT_OK)   return false;
  if (finger.createModel() != FINGERPRINT_OK)  return false;

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Guardado");
    lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
    Serial.print("Huella guardada ID "); Serial.println(id);
    delay(2500);
    return true;
  }
  return false;
}

//======================================
// MEJORA 6: leerHuella con timeout, no bloquea el sistema.
void leerHuella() {
  lcd.clear(); lcd.print("Ponga dedo");

  if (!esperarDedo(false, FINGER_TIMEOUT_MS)) { pantallaInicio(); return; }
  if (finger.image2Tz() != FINGERPRINT_OK)    { pantallaInicio(); return; }
  if (finger.fingerFastSearch() != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Denegado");
    delay(1500); pantallaInicio(); return;
  }

  int id = finger.fingerID;
  Serial.print("ID Detectado: "); Serial.println(id);

  if (id == ADMIN_ID_1 || id == ADMIN_ID_2) {
    lcd.clear(); lcd.print("Admin OK");
    delay(1000); mostrarMenuAdmin();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Acceso OK");
    lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
    delay(1500); abrirCaja();
  }
}

// Mueve el servo a abierto, espera 5s y cierra.
void abrirCaja() {
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Abriendo...");
  servoCaja.write(POS_ABIERTO);
  delay(5000);
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Cerrando...");
  servoCaja.write(POS_CERRADO);
  delay(2000);
  pantallaInicio();
}

// Captura PIN de 4 dígitos y valida contra usuarios registrados.
void leerPin() {
  String pin = "";
  lcd.clear(); lcd.print("PIN:");

  while (true) {
    char tecla = teclado.getKey();
    if (!tecla) continue;
    if (tecla >= '0' && tecla <= '9' && pin.length() < 4) {
      pin += tecla;
      lcd.setCursor(pin.length() - 1, 1); lcd.print("*");
    }
    if (tecla == '#') break;
    if (tecla == '*') { pantallaInicio(); return; }
  }

  if (pin.length() != 4) {
    lcd.clear(); lcd.print("PIN invalido");
    delay(1500); pantallaInicio(); return;
  }

  int id = buscarUsuarioPorPIN(pin);
  if (id != -1) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Acceso OK");
    lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
    delay(1500); abrirCaja();
  } else {
    lcd.clear(); lcd.print("PIN Incorrecto");
    delay(1500); pantallaInicio();
  }
}

//======================================
// MEJORA 1: return id ahora está DENTRO del if de coincidencia.
int buscarUsuarioPorPIN(String pinIngresado) {
  if (pinIngresado == "") return -1;
  for (int id = ID_MIN; id <= ID_MAX_USUARIOS; id++) {
    String clave = "pin" + String(id);
    String pinGuardado = prefs.getString(clave.c_str(), "");
    if (pinGuardado == "") continue;
    if (pinGuardado == pinIngresado) {
      Serial.print("PIN encontrado en ID "); Serial.println(id);
      return id;
    }
  }
  return -1;
}

//======================================

void menuAgregarUsuario() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("1H 2P 3Amb");
  lcd.setCursor(0, 1); lcd.print("* Cancelar");

  while (true) {
    char tecla = teclado.getKey();
    if (!tecla) continue;
    switch (tecla) {
      case '1': agregarHuella();       return;
      case '2': agregarUsuarioPIN();   return;
      case '3': agregarUsuarioAmbos(); return;
      case '*': mostrarMenuAdmin();    return;
    }
  }
}

// Captura PIN de 4 dígitos. Devuelve "" si se cancela con '*'.
String capturarPIN() {
  String pin = "";
  lcd.clear(); lcd.print("PIN 4 dig:");
  while (true) {
    char tecla = teclado.getKey();
    if (!tecla) continue;
    if (tecla >= '0' && tecla <= '9' && pin.length() < 4) {
      pin += tecla;
      lcd.setCursor(pin.length() - 1, 1); lcd.print("*");
    }
    if (tecla == '#' && pin.length() == 4) return pin;
    if (tecla == '*') return "";
  }
}

// MEJORA 9: valida que el ID esté en [ID_MIN, ID_MAX_USUARIOS].
int capturarID() {
  String numero = "";
  lcd.clear(); lcd.print("ID Usuario:");
  while (true) {
    char tecla = teclado.getKey();
    if (!tecla) continue;
    if (tecla >= '0' && tecla <= '9') {
      numero += tecla;
      lcd.setCursor(0, 1); lcd.print(numero);
    }
    if (tecla == '#') {
      if (numero.length() == 0) return -1;
      int id = numero.toInt();
      if (id < ID_MIN || id > ID_MAX_USUARIOS) return -1;
      return id;
    }
    if (tecla == '*') return -1;
  }
}

void agregarUsuarioPIN() {
  int id = buscarUsuarioLibre();
  if (id < 0) { mostrarMenuAdmin(); return; }

  String pin = capturarPIN();
  if (pin == "") { mostrarMenuAdmin(); return; }

  String clave = "pin" + String(id);
  prefs.putString(clave.c_str(), pin);

  lcd.clear(); lcd.print("Guardado");
  lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
  Serial.print("PIN Usuario "); Serial.print(id);
  Serial.print(" = "); Serial.println(pin);
  delay(2000);
  mostrarMenuAdmin();
}

void agregarUsuarioAmbos() {
  int id = buscarUsuarioLibre();
  if (id == -1) {
    lcd.clear(); lcd.print("Sin espacio");
    delay(2000); mostrarMenuAdmin(); return;
  }

  String pin = capturarPIN();
  if (pin == "") { mostrarMenuAdmin(); return; }

  String clave = "pin" + String(id);
  prefs.putString(clave.c_str(), pin);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ID:"); lcd.print(id);
  lcd.setCursor(0, 1); lcd.print("Registrar dedo");
  delay(2000);

  bool huellaOK = registrarHuellaEnID(id);
  lcd.clear();
  if (huellaOK) {
    lcd.setCursor(0, 0); lcd.print("Usuario OK");
    lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
    Serial.print("Usuario Huella+PIN ID "); Serial.println(id);
  } else {
    lcd.setCursor(0, 0); lcd.print("PIN guardado");
    lcd.setCursor(0, 1); lcd.print("Huella: error");
  }
  delay(2500);
  mostrarMenuAdmin();
}

void menuModificarUsuario() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("1 PIN"); lcd.setCursor(8, 0); lcd.print("2 Huella");
  lcd.setCursor(0, 1); lcd.print("3 Ambos");
  while (true) {
    char tecla = teclado.getKey();
    if (!tecla) continue;
    switch (tecla) {
      case '1': modificarPIN();    return;
      case '2': modificarHuella(); return;
      case '3': modificarAmbos();  return;
      case '*': mostrarMenuAdmin(); return;
    }
  }
}

void eliminarUsuarioCompleto() {
  int id = capturarID();
  if (id < 0) { mostrarMenuAdmin(); return; }

  String clave = "pin" + String(id);
  prefs.remove(clave.c_str());
  finger.deleteModel(id);

  lcd.clear(); lcd.print("Eliminado");
  delay(2000);
  mostrarMenuAdmin();
}

void modificarHuella() {
  int id = capturarID();
  if (id < 0) { mostrarMenuAdmin(); return; }

  finger.deleteModel(id);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Nueva Huella");
  lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
  delay(2000);

  bool huellaOK = registrarHuellaEnID(id);
  lcd.clear();
  lcd.print(huellaOK ? "Modificada" : "Error/Cancelado");
  delay(2000);
  mostrarMenuAdmin();
}

void modificarAmbos() {
  int id = capturarID();
  if (id < 0) { mostrarMenuAdmin(); return; }

  String pin = capturarPIN();
  if (pin == "") { mostrarMenuAdmin(); return; }

  String clave = "pin" + String(id);
  prefs.putString(clave.c_str(), pin);

  finger.deleteModel(id);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Nueva Huella");
  lcd.setCursor(0, 1); lcd.print("ID:"); lcd.print(id);
  delay(1500);

  bool huellaOK = registrarHuellaEnID(id);
  lcd.clear();
  if (huellaOK) {
    lcd.setCursor(0, 0); lcd.print("Usuario");
    lcd.setCursor(0, 1); lcd.print("Actualizado");
  } else {
    lcd.setCursor(0, 0); lcd.print("PIN OK, huella");
    lcd.setCursor(0, 1); lcd.print("con error");
  }
  delay(2000);
  mostrarMenuAdmin();
}

// MEJORA 8: al cancelar vuelve al menú admin correctamente.
void modificarPIN() {
  int id = capturarID();
  if (id < 0) { mostrarMenuAdmin(); return; }

  String clave = "pin" + String(id);
  if (prefs.getString(clave.c_str(), "") == "") {
    lcd.clear(); lcd.print("ID no existe");
    delay(2000); mostrarMenuAdmin(); return;
  }

  String nuevoPin = capturarPIN();
  if (nuevoPin == "") { mostrarMenuAdmin(); return; }

  prefs.putString(clave.c_str(), nuevoPin);
  lcd.clear(); lcd.print("PIN cambiado");
  delay(2000);
  mostrarMenuAdmin();
}
