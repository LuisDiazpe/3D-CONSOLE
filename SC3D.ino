// SC3D.ino - Space Shooter 3D | LOIKO CONSOLE
// Librerias: Adafruit SSD1306 >=2.5  |  Adafruit GFX >=1.11
// Pines:
//   OLED  VCC->5V  GND->GND  SDA->A4  SCL->A5
//   JOY   +5V->5V  GND->GND  VRx->A0  VRy->A1  SW->D2

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

#define PIN_JX  A0
#define PIN_JY  A1
#define PIN_BTN 2
#define RJX()   (analogRead(PIN_JX) - 512)
#define RJY()   (analogRead(PIN_JY) - 512)
#define RBTN()  (digitalRead(PIN_BTN) == LOW)
#define DEAD    110
#define EE_LO   0
#define EE_HI2  1

// ============================================================
// TABLA SENO en PROGMEM — 64 entradas, rango 0..255 -> sin*127
// Elimina llamadas a sin()/cos() float en el boot, ahorrando
// ~3-4 KB de flash en AVR. El juego sigue usando float sin()
// porque ya estaba optimizado por el compilador ahi.
// ============================================================
static const int8_t PROGMEM SIN64[64] = {
    0,  12,  25,  37,  49,  60,  71,  81,
   90,  98, 106, 112, 117, 122, 125, 126,
  127, 126, 125, 122, 117, 112, 106,  98,
   90,  81,  71,  60,  49,  37,  25,  12,
    0, -12, -25, -37, -49, -60, -71, -81,
  -90, -98,-106,-112,-117,-122,-125,-126,
 -127,-126,-125,-122,-117,-112,-106, -98,
  -90, -81, -71, -60, -49, -37, -25, -12
};

// isin/icos: angulo 0..255 -> valor -127..127
inline int8_t isin(uint8_t a){ return (int8_t)pgm_read_byte(&SIN64[(a>>2)&63]); }
inline int8_t icos(uint8_t a){ return isin(a+64); }

// Multiplicacion escalada: (int8_t * int8_t) / 128 -> int8_t
// Usada para proyectar puntos en el boot sin floats
inline int8_t imul(int8_t a, int8_t b){
  return (int8_t)((int16_t)a * b >> 7);
}

// PROYECCION 3D (juego — usa float)
static bool projAt(int16_t wx, int16_t wy, int16_t wz,
                   int8_t ocx, int8_t ocy, int16_t fov,
                   int8_t *sx, int8_t *sy) {
  if (wz < 2) return false;
  int32_t s = (int32_t)fov * 16L / wz;
  *sx = (int8_t)constrain((int32_t)ocx + (int32_t)wx*s/16L, -127,127);
  *sy = (int8_t)constrain((int32_t)ocy - (int32_t)wy*s/16L,  -63, 63);
  return true;
}

#define GCX 64
#define GCY 38
#define GFOV 200
static bool gProj(int16_t wx,int16_t wy,int16_t wz,int8_t*sx,int8_t*sy){
  return projAt(wx,wy,wz,GCX,GCY,GFOV,sx,sy);
}

// MODELO NAVE
static const int8_t PROGMEM PV[][3]={
  {  0,  0, 14},
  { -7, -3, -8},
  {  7, -3, -8},
  {  0,  4, -8},
  {-18, -2,  0},
  { 18, -2,  0},
  { -7, -3,  2},
  {  7, -3,  2},
  {  0,  1,  5},
};
static const uint8_t PROGMEM PE[][2]={
  {0,1},{0,2},{0,3},{1,2},{1,3},{2,3},
  {4,6},{6,1},{5,7},{7,2},{4,5},{8,0},{8,3}
};
#define N_PV 9
#define N_PE 13

static float bvx[N_PV],bvy[N_PV],bvz[N_PV];

static void drawShip(float pitch,float yaw,float roll,float sc,
                     int8_t ocx,int8_t ocy,int16_t fov,int16_t camZ){
  float sp=sin(pitch),cp=cos(pitch);
  float sy2=sin(yaw),  cy=cos(yaw);
  float sr=sin(roll),  cr=cos(roll);
  for(uint8_t i=0;i<N_PV;i++){
    float x=(int8_t)pgm_read_byte(&PV[i][0])*sc;
    float y=(int8_t)pgm_read_byte(&PV[i][1])*sc;
    float z=(int8_t)pgm_read_byte(&PV[i][2])*sc;
    float tx=x*cy+z*sy2; float tz=-x*sy2+z*cy; x=tx;z=tz;
    float ty=y*cp-z*sp;  float tz2=y*sp+z*cp;  y=ty;z=tz2;
    float tx2=x*cr-y*sr; float ty2=x*sr+y*cr;  x=tx2;y=ty2;
    bvx[i]=x;bvy[i]=y;bvz[i]=z;
  }
  for(uint8_t e=0;e<N_PE;e++){
    uint8_t a=pgm_read_byte(&PE[e][0]);
    uint8_t b=pgm_read_byte(&PE[e][1]);
    int8_t sx1,sy1,sx2,sy22;
    bool oa=projAt((int16_t)bvx[a],(int16_t)bvy[a],(int16_t)(bvz[a]+camZ),ocx,ocy,fov,&sx1,&sy1);
    bool ob=projAt((int16_t)bvx[b],(int16_t)bvy[b],(int16_t)(bvz[b]+camZ),ocx,ocy,fov,&sx2,&sy22);
    if(oa&&ob) oled.drawLine(sx1,sy1,sx2,sy22,SSD1306_WHITE);
  }
}

// MODELO ASTEROIDE
static const int8_t PROGMEM AV[][3]={
  { 0,16, 0},{16, 0, 0},{ 0,-16, 0},{-16, 0, 0},
  { 0, 0, 8},{ 0, 0,-8}
};
static const uint8_t PROGMEM AE[][2]={
  {0,1},{1,2},{2,3},{3,0},
  {4,0},{4,1},{4,2},{4,3},
  {5,0},{5,1},{5,2},{5,3}
};
#define N_AV 6
#define N_AE 12

static void drawAsteroid(int16_t wx,int16_t wy,int16_t wz,
                         uint8_t ang,float sc,
                         int8_t ocx,int8_t ocy,int16_t fov){
  if(wz<2) return;
  float fa=(float)ang*0.02454f;
  int16_t sa=(int16_t)(sin(fa)*64.0f);
  int16_t ca=(int16_t)(cos(fa)*64.0f);
  int8_t px2[N_AV],py2[N_AV]; bool ok[N_AV];
  for(uint8_t i=0;i<N_AV;i++){
    int16_t lx=(int16_t)((int8_t)pgm_read_byte(&AV[i][0])*sc);
    int16_t ly=(int16_t)((int8_t)pgm_read_byte(&AV[i][1])*sc);
    int16_t lz=(int16_t)((int8_t)pgm_read_byte(&AV[i][2])*sc);
    int16_t rx=(int16_t)((int32_t)lx*ca/64-(int32_t)ly*sa/64);
    int16_t ry=(int16_t)((int32_t)lx*sa/64+(int32_t)ly*ca/64);
    ok[i]=projAt(wx+rx,wy+ry,wz+lz,ocx,ocy,fov,&px2[i],&py2[i]);
  }
  for(uint8_t e=0;e<N_AE;e++){
    uint8_t a=pgm_read_byte(&AE[e][0]);
    uint8_t b=pgm_read_byte(&AE[e][1]);
    if(ok[a]&&ok[b])
      oled.drawLine(px2[a],py2[a],px2[b],py2[b],SSD1306_WHITE);
  }
}

// STARFIELD
#define N_STARS 24
struct StarPt{int16_t x,y,z;};
static StarPt stars[N_STARS];

static void starsInit(){
  for(uint8_t i=0;i<N_STARS;i++){
    stars[i].x=(int16_t)random(-200,200);
    stars[i].y=(int16_t)random(-100,100);
    stars[i].z=(int16_t)random(20,400);
  }
}

static void starsDraw(uint8_t spd,int16_t ox,int16_t oy,
                      int8_t ocx,int8_t ocy,int16_t fov,
                      int8_t clipY1,int8_t clipY2){
  for(uint8_t i=0;i<N_STARS;i++){
    stars[i].z-=spd;
    if(stars[i].z<2){
      stars[i].x=(int16_t)random(-200,200);
      stars[i].y=(int16_t)random(-100,100);
      stars[i].z=400;
    }
    int8_t sx,sy;
    if(!projAt(stars[i].x-ox/8,stars[i].y-oy/8,stars[i].z,
               ocx,ocy,fov,&sx,&sy)) continue;
    if(sy<clipY1||sy>clipY2) continue;
    if(stars[i].z<80) oled.drawLine(sx-2,sy,sx,sy,SSD1306_WHITE);
    else oled.drawPixel(sx,sy,SSD1306_WHITE);
  }
}

// ============================================================
// BOOT ANIMATION — LOIKO CONSOLE
//
// La clave del 3D: cada vertice de la esfera se rota en
// DOS ejes (yaw + pitch) antes de proyectarse. Esto hace que
// los meridianos se vean curvarse en profundidad y los anillos
// de latitud se inclinen, dando la sensacion de una
// esfera solida girando 
//
// Toda la trigonometria usa isin()/icos() con la tabla SIN64,
// sin ninguna operacion float — cabe en el UNO junto al juego.
//
// Secuencia (~6 segundos + espera input):
//   0-500ms   : genesis — punto que explota en circulo
//   500-2500ms : esfera 3D — crece y gira en yaw+pitch 
//   2500-4000ms: portal — esfera explota, anillos + rayos 
//   4000-5200ms: logo — letras LOIKO caen una a una 
//   5200ms+   : PRESS ANY KEY parpadeando
//
// Proyeccion entera usada en la esfera:
//   Para un punto (lx,ly,lz) en espacio local radio R:
//   1. Rotar en Y (yaw):  rx = lx*cy - lz*sy
//                         rz = lx*sy + lz*cy
//   2. Rotar en X (pitch): ry = ly*cp - rz*sp
//                          rz2= ly*sp + rz*cp
//   3. Proyectar:  wz = rz2 + FOV_DIST
//                  sx = CX + rx*FOV_DIST/wz
//                  sy = CY - ry*FOV_DIST/wz
//   Todo en int16, escalado *128 para precision sin floats.
// ============================================================

// Fuente pixel 5x7 para LOIKO — columnas como bitmask
// bit0=fila superior, bit6=fila inferior
static const uint8_t PROGMEM FL[5]={0x7F,0x40,0x40,0x40,0x40}; // L
static const uint8_t PROGMEM FO[5]={0x3E,0x41,0x41,0x41,0x3E}; // O
static const uint8_t PROGMEM FI[5]={0x41,0x41,0x7F,0x41,0x41}; // I
static const uint8_t PROGMEM FK[5]={0x7F,0x08,0x14,0x22,0x41}; // K

static const uint8_t* const PROGMEM LF[5]={FL,FO,FI,FK,FO};
static const int8_t PROGMEM LX[5]={24,38,52,66,80};
#define LOIKO_Y  22
#define LOIKO_SC 2

static void bootLetter(const uint8_t* fd, int8_t cx, int8_t cy, uint8_t sc){
  for(uint8_t col=0;col<5;col++){
    uint8_t cd=pgm_read_byte(&fd[col]);
    for(uint8_t row=0;row<7;row++){
      if(cd&(1<<row)){
        int8_t px=cx-(int8_t)(5*sc/2)+(int8_t)(col*sc);
        int8_t py=cy-(int8_t)(7*sc/2)+(int8_t)(row*sc);
        if(sc==1) oled.drawPixel(px,py,SSD1306_WHITE);
        else      oled.fillRect(px,py,sc,sc,SSD1306_WHITE);
      }
    }
  }
}

//Esfera wireframe 3D con rotacion en dos ejes 
//Dibuja la esfera como una cuadricula de 4 latitudes x 4 longitudes
//Cada curva se muestrea en 18 puntos cada 20 grados
//yaw   = angulo rotacion eje Y (0..255)
//pitch = angulo rotacion eje X (0..255)
//r     = radio de la esfera en unidades de mundo (escala 1:1 con px)
//cx,cy = centro en pantalla
//fdist = distancia focal (mayor = menos perspectiva, tipico 50-70)
//Precision: se trabaja en int16 escalado x128.
//rx_s = (lx*cy - lz*sy) en [-128R..128R], se divide /128 al proyectar
//El radio maximo util con int16 y fdist=60 es ~30px antes de overflow
static void bootSphere(uint8_t yaw, uint8_t pitch, uint8_t r,
                       int8_t cx, int8_t cy, uint8_t fdist){
  // Componentes de rotacion (escala 127)
  int8_t sy=isin(yaw),   cy2=icos(yaw);
  int8_t sp=isin(pitch), cp=icos(pitch);

  //Anillos de latitud en paralelo
  // 5 latitudes: -72, -36, 0, 36, 72 grados en escala 256
  // sin(-72)*127=-121, sin(-36)*127=-75, 0, 75, 121
  // cos(-72)*127= 39,  cos(-36)*127=103, 127
  static const int8_t PROGMEM LAT_Y[5]={-24,-15, 0,15,24}; // yr = sin(lat)*r aprox
  static const uint8_t PROGMEM LAT_R[5]={  8, 18,24,18, 8}; // rr = cos(lat)*r aprox

  for(uint8_t li=0;li<5;li++){
    // yr y rr escalados al radio actual
    int8_t  yr=(int8_t)((int16_t)(int8_t)pgm_read_byte(&LAT_Y[li])*(int16_t)r/24);
    uint8_t rr=(uint8_t)((uint16_t)pgm_read_byte(&LAT_R[li])*(uint16_t)r/24);
    if(rr<2) continue;
    int8_t px0=0,py0=0; bool first=true;
    for(uint8_t seg=0;seg<=18;seg++){
      uint8_t sa=(uint8_t)(seg*14); // 18 segmentos ~= 256/18=14.2
      // Punto en espacio local del anillo
      // lx = cos(sa)*rr, ly = yr (constante), lz = sin(sa)*rr
      int16_t lx=(int16_t)icos(sa)*(int16_t)rr; // escala *127
      int16_t lz=(int16_t)isin(sa)*(int16_t)rr;
      int16_t ly=(int16_t)yr*127;
      // Rotacion Y (yaw): rx = lx*cy2 - lz*sy  (escala *127^2)
      int16_t rx=(int16_t)(((int32_t)lx*cy2 - (int32_t)lz*sy)>>7);
      int16_t rz=(int16_t)(((int32_t)lx*sy  + (int32_t)lz*cy2)>>7);
      // Rotacion X (pitch): ry = ly*cp - rz*sp
      int16_t ry=(int16_t)(((int32_t)ly*cp  - (int32_t)rz*sp)>>7);
      int16_t rz2=(int16_t)(((int32_t)ly*sp + (int32_t)rz*cp)>>7);
      // Proyeccion perspectiva
      int16_t wz=rz2/127 + (int16_t)fdist;
      if(wz<4){first=true;continue;}
      int8_t sx=(int8_t)constrain((int32_t)cx + (int32_t)(rx/127)*(int32_t)fdist/wz, 0,127);
      int8_t sy2=(int8_t)constrain((int32_t)cy - (int32_t)(ry/127)*(int32_t)fdist/wz, 0,63);
      if(!first) oled.drawLine(px0,py0,sx,sy2,SSD1306_WHITE);
      px0=sx; py0=sy2; first=false;
    }
  }

  //Meridianos longitudes
  //6 meridianos cada 30 grados (64/6 ~= 43 en escala 256)
  for(uint8_t mi=0;mi<6;mi++){
    uint8_t mOff=(uint8_t)(mi*43);
    int8_t px0=0,py0=0; bool first=true;
    for(uint8_t seg=0;seg<=18;seg++){
      uint8_t sa=(uint8_t)(seg*14);
      //Meridiano: recorre de polo a polo en el plano definido por mOff
      //lx = cos(mOff)*sin(sa)*r
      //ly = cos(sa)*r
      //lz = sin(mOff)*sin(sa)*r
      int16_t sinSa=(int16_t)isin(sa);
      int16_t cosSa=(int16_t)icos(sa);
      int16_t lx=(int16_t)icos(mOff)*sinSa*(int16_t)r>>7;
      int16_t ly=cosSa*(int16_t)r;
      int16_t lz=(int16_t)isin(mOff)*sinSa*(int16_t)r>>7;
      //Rotacion Y
      int16_t rx=(int16_t)(((int32_t)lx*cy2 - (int32_t)lz*sy)>>7);
      int16_t rz=(int16_t)(((int32_t)lx*sy  + (int32_t)lz*cy2)>>7);
      //Rotacion X
      int16_t ry=(int16_t)(((int32_t)ly*cp  - (int32_t)rz*sp)>>7);
      int16_t rz2=(int16_t)(((int32_t)ly*sp + (int32_t)rz*cp)>>7);
      //Proyeccion
      int16_t wz=rz2/127 + (int16_t)fdist;
      if(wz<4){first=true;continue;}
      int8_t sx=(int8_t)constrain((int32_t)cx + (int32_t)(rx/127)*(int32_t)fdist/wz, 0,127);
      int8_t sy2=(int8_t)constrain((int32_t)cy - (int32_t)(ry/127)*(int32_t)fdist/wz, 0,63);
      if(!first) oled.drawLine(px0,py0,sx,sy2,SSD1306_WHITE);
      px0=sx; py0=sy2; first=false;
    }
  }
}

//Rayos radiales del portal
static void bootRays(uint8_t n, uint8_t r1, uint8_t r2,
                     uint8_t aOff, int8_t cx, int8_t cy){
  for(uint8_t i=0;i<n;i++){
    uint8_t a=aOff+(uint8_t)(i*(256/n));
    int8_t x1=cx+(int8_t)((int16_t)icos(a)*(int16_t)r1>>7);
    int8_t y1=cy+(int8_t)((int16_t)isin(a)*(int16_t)r1>>7);
    int8_t x2=cx+(int8_t)((int16_t)icos(a)*(int16_t)r2>>7);
    int8_t y2=cy+(int8_t)((int16_t)isin(a)*(int16_t)r2>>7);
    oled.drawLine(x1,y1,x2,y2,SSD1306_WHITE);
  }
}

//Hiper-estrellas con LCG (sin array global, ahorra SRAM)
static uint16_t hSeed=0xA5F1;
static void hyperDraw(int8_t cx, int8_t cy){
  uint16_t s=hSeed;
  for(uint8_t i=0;i<14;i++){
    s=(uint16_t)(s*25173U+13849U);
    uint8_t ang=(uint8_t)(s>>8);
    uint8_t r0=(uint8_t)(s&0x3F)+3;
    uint8_t len=(uint8_t)((s>>4)&0xF)+5;
    int8_t x1=cx+(int8_t)((int16_t)icos(ang)*(int16_t)r0>>7);
    int8_t y1=cy+(int8_t)((int16_t)isin(ang)*(int16_t)r0>>7);
    uint8_t r1=r0+len; if(r1>70) continue;
    int8_t x2=cx+(int8_t)((int16_t)icos(ang)*(int16_t)r1>>7);
    int8_t y2=cy+(int8_t)((int16_t)isin(ang)*(int16_t)r1>>7);
    if(x2<0||x2>127||y2<0||y2>63) continue;
    oled.drawLine(x1,y1,x2,y2,SSD1306_WHITE);
  }
  hSeed=(uint16_t)(hSeed*25173U+13849U);
}

//Funcion principal de boot
static void bootAnimation(){
  const int8_t  CX=64, CY=32;
  const uint8_t FDIST=58; // distancia focal: da buena perspectiva a r=24

  //Estado de la esfera: yaw y pitch giran a velocidades distintas
  //para que la rotacion se vea inequivocamente 3D (no circular).
  uint8_t sYaw=0, sPitch=200; //pitch empieza descentrado para ver el 3D de entrada
  uint8_t ringOff=0;
  bool blinkOn=true; uint8_t blinkCnt=0;

  unsigned long tStart=millis();

  while(true){
    unsigned long el=millis()-tStart;
    oled.clearDisplay();

    //Rotar la esfera continuamente en ambos ejes con
    //velocidades distintas
    //Yaw rapido (3/frame), pitch lento (1/frame) y con
    //direccion opuesta: la esfera traza una trayectoria
    //tipo "precesion giroscopica" 
    sYaw  += 3;
    sPitch += 1;

    //FASE 0: Genesis (0-500ms
    if(el<700){
      uint8_t rr=(uint8_t)(el*14/700); // crece a r=14
      if(rr<2){
        if((el/55)%2==0) oled.drawPixel(CX,CY,SSD1306_WHITE);
      } else {
        //Dos circulos concentricos para dar grosor
        oled.drawCircle(CX,CY,rr,  SSD1306_WHITE);
        if(rr>2) oled.drawCircle(CX,CY,rr-1,SSD1306_WHITE);
        //Cruz central en los primeros 200ms
        if(el<300){
          oled.drawLine(CX-3,CY,CX+3,CY,SSD1306_WHITE);
          oled.drawLine(CX,CY-3,CX,CY+3,SSD1306_WHITE);
        }
      }
    }

    //FASE 1: Esfera 3D (500-2500ms = 2 segundos)
    // Radio crece de 0 a 24 en los primeros 600ms, luego se queda fijo
    // La esfera rota durante 2 segundos completos para que el ojo
    else if(el<7200){
      uint16_t t1=el-700;
      uint8_t r;
      if(t1<900){
        // Easing cuadratico inverso: crece rapido al principio
        uint16_t p=t1*128/900; // 0..128
        r=(uint8_t)(24-(uint32_t)(128-p)*(128-p)*24/16384UL);
      } else {
        r=24;
      }
      if(r>2) bootSphere(sYaw,sPitch,r,CX,CY,FDIST);
    }

    //FASE 2: Portal wormhole (2500-4000ms = 1.5s)
    else if(el<9800){
      uint16_t t2=el-7200;
      //Esfera residual: sigue rotando pero encogiendo rapidamente
      if(t2<300){
        uint8_t r=(uint8_t)(24-(uint32_t)24*t2/300);
        if(r>2) bootSphere(sYaw,sPitch,r,CX,CY,FDIST);
      }
      //5 anillos expansivos escalonados
      for(uint8_t ri=0;ri<5;ri++){
        int16_t td=(int16_t)t2-(int16_t)(ri*150);
        if(td<0) continue;
        uint8_t rr2=(uint8_t)min(80,(int16_t)8+(int32_t)td*72/2400);
        oled.drawCircle(CX,CY,rr2,SSD1306_WHITE);
      }
      //8 rayos giratorios (empiezan a los 150ms para dar tiempo al "estallido")
      if(t2>200){
        ringOff+=3;
        bootRays(8,6,38,ringOff,CX,CY);
      }
      //Estrellas hiper-velocidad
      hyperDraw(CX,CY);
      //Agujero oscuro en el centro con borde brillante
      oled.fillCircle(CX,CY,5,SSD1306_BLACK);
      oled.drawCircle(CX,CY,5,SSD1306_WHITE);
    }

    //FASE 3: Logo emerge (4000-5200ms = 1.2s)
    else if(el<11400){
      uint16_t t3=el-9800;
      //Anillo residual que se expande y desaparece
      if(t3<500){
        uint8_t rr3=(uint8_t)(55+(uint32_t)t3*20/500);
        oled.drawCircle(CX,CY,rr3,SSD1306_WHITE);
      }
      //Letras LOIKO caen desde arriba una a una, separadas 90ms
      for(uint8_t li=0;li<5;li++){
        int16_t td=(int16_t)t3-(int16_t)(li*130);
        if(td<0) continue;
        int8_t py;
        if(td>=300){
          py=LOIKO_Y;
        } else {
          //Easing suavizado: sale rapido
          uint16_t inv=300-td;
          int16_t delta=(int16_t)((uint32_t)inv*inv*(LOIKO_Y+14)/90000UL);
          py=(int8_t)(LOIKO_Y-delta);
          if(py<-10) py=-10;
        }
        bootLetter(
          (const uint8_t*)pgm_read_word(&LF[li]),
          (int8_t)pgm_read_byte(&LX[li])+(int8_t)(LOIKO_SC*5/2),
          py, LOIKO_SC
        );
      }
      //Linea y "CONSOLE" aparecen cuando todas las letras ya llegaron
      if(t3>750){
        oled.drawLine(20,31,108,31,SSD1306_WHITE);
        oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(29,34); oled.print(F("C O N S O L E"));
      }
    }

    //            Press Any Key
    else {
      for(uint8_t li=0;li<5;li++){
        bootLetter(
          (const uint8_t*)pgm_read_word(&LF[li]),
          (int8_t)pgm_read_byte(&LX[li])+(int8_t)(LOIKO_SC*5/2),
          LOIKO_Y, LOIKO_SC
        );
      }
      oled.drawLine(20,31,108,31,SSD1306_WHITE);
      oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(29,34); oled.print(F("C O N S O L E"));

      blinkCnt++;
      if(blinkCnt>=16){blinkCnt=0;blinkOn=!blinkOn;}
      if(blinkOn){
        oled.setCursor(16,52);
        oled.print(F("-- PRESS ANY KEY --"));
      }
      //Estrellas decorativas en las cuatro esquinas
      oled.drawPixel(10,10,SSD1306_WHITE); oled.drawPixel(12,8, SSD1306_WHITE);
      oled.drawPixel(8, 12,SSD1306_WHITE); oled.drawPixel(10,54,SSD1306_WHITE);
      oled.drawPixel(116,10,SSD1306_WHITE);oled.drawPixel(118,8,SSD1306_WHITE);
      oled.drawPixel(120,12,SSD1306_WHITE);oled.drawPixel(118,54,SSD1306_WHITE);

      oled.display();
      delay(30);
      if(RBTN()){ delay(200); return; }
      int jx=RJX(), jy=RJY();
      if(abs(jx)>DEAD||abs(jy)>DEAD){ delay(200); return; }
      continue;
    }

    oled.display();
    delay(30);
  }
}


// MENU — demo cinematica 3D
#define DCX  64
#define DCY  18
#define DFOV 150

static float   dCamAng=0.0f, dCamAngV=0.018f;
static float   dBulZ=999.0f;
static uint8_t dA1=0, dA2=80;
static int16_t dAst1Z=180, dAst2Z=280;
static uint8_t dExplR=0;
static int8_t  dExplX=0, dExplY=0;
static bool    dExplOn=false;
static unsigned long dLastShot=0;

static void menuDemoInit(){
  dCamAng=0; dBulZ=999; dA1=0; dA2=80;
  dAst1Z=180; dAst2Z=280; dExplOn=false; dExplR=0;
}

static void menuDemoDraw(unsigned long now){
  float cang=dCamAng;
  float sc2=sin(cang), cc2=cos(cang);
  dCamAng+=dCamAngV;
  if(dCamAng>6.283f) dCamAng-=6.283f;

  dAst1Z-=4; if(dAst1Z<-30) dAst1Z=200;
  dA1+=5; dA2+=3;
  dAst2Z-=2; if(dAst2Z<-30) dAst2Z=280;

  if(dBulZ>=999.0f&&now-dLastShot>900){
    dBulZ=15.0f; dLastShot=now; dExplOn=false;
  }
  if(dBulZ<999.0f){
    dBulZ+=22.0f;
    if(!dExplOn&&dBulZ>=(float)dAst1Z-15&&dBulZ<=(float)dAst1Z+15){
      dExplOn=true; dExplR=2;
      float ax=8.0f,ay=5.0f,az=(float)dAst1Z;
      float rx=ax*cc2+az*sc2, rz=-ax*sc2+az*cc2;
      int8_t esx,esy;
      if(projAt((int16_t)rx,(int16_t)ay,(int16_t)(rz+60),DCX,DCY,DFOV,&esx,&esy)){
        dExplX=esx; dExplY=constrain(esy,1,38);
      } else {dExplX=DCX;dExplY=DCY;}
      dBulZ=999.0f; dAst1Z=200;
    }
    if(dBulZ>320.0f) dBulZ=999.0f;
  }
  if(dExplOn){dExplR+=3;if(dExplR>16)dExplOn=false;}

  for(uint8_t i=0;i<N_STARS;i++){
    int8_t sx,sy;
    if(!projAt(stars[i].x,stars[i].y,stars[i].z,DCX,DCY,DFOV,&sx,&sy)) continue;
    if(sy<1||sy>38) continue;
    oled.drawPixel(sx,sy,SSD1306_WHITE);
  }

  drawShip(0.0f,-cang*0.3f,sin(cang*0.8f)*0.25f,1.0f,DCX,DCY,DFOV,60);

  if(dBulZ<999.0f){
    float bwz=dBulZ;
    float brx=bwz*sc2, brz=bwz*cc2;
    int8_t sx1,sy1,sx2,sy2;
    bool o1=projAt((int16_t)brx,0,(int16_t)(brz+60),DCX,DCY,DFOV,&sx1,&sy1);
    float bwz2=dBulZ-14.0f;
    bool o2=projAt((int16_t)(bwz2*sc2),0,(int16_t)(bwz2*cc2+60),DCX,DCY,DFOV,&sx2,&sy2);
    if(o1&&sy1>=1&&sy1<=38){
      if(o2&&sy2>=1&&sy2<=38) oled.drawLine(sx1,sy1,sx2,sy2,SSD1306_WHITE);
      else oled.drawPixel(sx1,sy1,SSD1306_WHITE);
    }
  }

  {
    float ax=8.0f,ay=5.0f,az=(float)dAst1Z;
    float rx=ax*cc2+az*sc2, rz=-ax*sc2+az*cc2;
    int16_t wz=(int16_t)(rz+60);
    if(wz>4){ int8_t tsx,tsy;
      if(projAt((int16_t)rx,(int16_t)ay,wz,DCX,DCY,DFOV,&tsx,&tsy)&&tsy>=1&&tsy<=38)
        drawAsteroid((int16_t)rx,(int16_t)ay,wz,dA1,0.9f,DCX,DCY,DFOV); }
  }
  {
    float ax=-10.0f,ay=-4.0f,az=(float)dAst2Z;
    float rx=ax*cc2+az*sc2, rz=-ax*sc2+az*cc2;
    int16_t wz=(int16_t)(rz+60);
    if(wz>4){ int8_t tsx,tsy;
      if(projAt((int16_t)rx,(int16_t)ay,wz,DCX,DCY,DFOV,&tsx,&tsy)&&tsy>=1&&tsy<=38)
        drawAsteroid((int16_t)rx,(int16_t)ay,wz,dA2,0.75f,DCX,DCY,DFOV); }
  }

  if(dExplOn&&dExplY>=1&&dExplY<=38){
    oled.drawCircle(dExplX,dExplY,dExplR,SSD1306_WHITE);
    if(dExplR>5) oled.drawCircle(dExplX,dExplY,dExplR-4,SSD1306_WHITE);
  }
}

// MENU PRINCIPAL
static uint8_t runMenu(){
  uint8_t sel=0; bool lb=false;
  unsigned long lnav=0;
  starsInit(); menuDemoInit();

  while(true){
    unsigned long now=millis();
    if(now-lnav>240){
      int jy=RJY();
      if(jy<-DEAD||jy>DEAD){sel=1-sel;lnav=now;}
    }
    bool btn=RBTN();
    if(btn&&!lb) return sel;
    lb=btn;

    oled.clearDisplay();
    menuDemoDraw(now);
    oled.drawLine(0,40,127,40,SSD1306_WHITE);

    oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    for(uint8_t i=0;i<2;i++){
      int8_t ty=43+i*11;
      if(i==sel){
        oled.fillRect(0,ty-1,128,11,SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
      } else oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(4,ty);
      oled.print(i==0?F("> JUGAR"):F("> HIGH SCORE"));
    }
    oled.setTextColor(SSD1306_WHITE);
    oled.display();
    delay(30);
  }
}

// JUEGO
#define CAM_DIST 60

static float bankX=0.0f,bankY=0.0f;
static int16_t plrX=0,plrY=0;

static void playerDraw(){
  drawShip(bankY,0.0f,-bankX,1.0f,GCX,GCY,GFOV,CAM_DIST);
}

#define N_ENE 5
struct Asteroid{int16_t ex,ey,ez;uint8_t ang,angSpd,spd;bool on;};
static Asteroid ast[N_ENE];

static void asteroidSpawn(){
  for(uint8_t i=0;i<N_ENE;i++){
    if(!ast[i].on){
      ast[i]={(int16_t)random(-65,65),(int16_t)random(-32,32),300,
               (uint8_t)random(0,256),(uint8_t)random(3,8),(uint8_t)random(4,10),true};
      return;
    }
  }
}

#define N_BULL 6
struct Laser{int16_t ex,ey,ez;bool on;};
static Laser laser[N_BULL];

static void laserFire(){
  for(uint8_t i=0;i<N_BULL;i++)
    if(!laser[i].on){laser[i]={plrX,plrY,18,true};return;}
}

static void laserDraw(uint8_t idx){
  Laser &l=laser[idx];
  int8_t sx1,sy1,sx2,sy2;
  bool o1=gProj(l.ex,l.ey,(int16_t)(l.ez+CAM_DIST),&sx1,&sy1);
  bool o2=gProj(l.ex,l.ey,(int16_t)(l.ez+CAM_DIST-25),&sx2,&sy2);
  if(o1&&o2){
    oled.drawLine(sx1,sy1,sx2,sy2,SSD1306_WHITE);
    oled.drawLine(sx1,sy1+1,sx2,sy2+1,SSD1306_WHITE);
  } else if(o1) oled.drawLine(sx1-4,sy1,sx1+4,sy1,SSD1306_WHITE);
}

#define N_EXPL 4
struct Expl{int8_t sx,sy;uint8_t r;bool on;};
static Expl expl[N_EXPL];

static void explTrigger(int8_t sx,int8_t sy){
  for(uint8_t i=0;i<N_EXPL;i++)
    if(!expl[i].on){expl[i]={sx,sy,3,true};return;}
}

static int16_t gScore=0;
static int8_t  gLives=0;

static void runGame(){
  plrX=0;plrY=0;gScore=0;gLives=3;
  bankX=0.0f;bankY=0.0f;
  for(uint8_t i=0;i<N_BULL;i++) laser[i].on=false;
  for(uint8_t i=0;i<N_ENE; i++) ast[i].on  =false;
  for(uint8_t i=0;i<N_EXPL;i++) expl[i].on =false;
  starsInit();

  for(int8_t n=3;n>=0;n--){
    oled.clearDisplay();oled.setTextSize(3);oled.setTextColor(SSD1306_WHITE);
    if(n>0){oled.setCursor(55,18);oled.print(n);}
    else   {oled.setCursor(28,18);oled.print(F("GO!"));}
    oled.display();delay(600);
  }

  unsigned long tShot=0,tSpawn=millis(),tFrame=millis();
  uint16_t spawnMs=1200;

  //Estado pausa — declarados al mismo nivel que lb para que nO sean static locales con estado persistente entre pausas
  bool paused=false;
  uint8_t pSel=0;
  bool lb=false;        //ultimo estado boton (toggle pausa)
  bool backToMenu=false;

  // gameOver=true  -> mostrar pantalla Game Over al salir
  // backToMenu=true -> volver al menu silenciosamente
  // Ambos false al inicio; solo uno se pone true al romper el loop.
  bool gameOver=false;

  while(true){
    while(millis()-tFrame<35);
    tFrame=millis();
    unsigned long now=millis();

    //Toggle pausa
    bool btn=RBTN();
    if(btn&&!lb){
      paused=!paused;
      if(paused){ pSel=0; }
    }
    lb=btn;

    //Bloque pausa
    if(paused){
      //Esperar a que el boton se suelte para no propagar el flanco
      while(RBTN()) delay(10);
      delay(80);

      //Loop bloqueante propio con deteccion de flanco limpia
      bool btnWasUp=true;
      unsigned long tNavP=0;
      pSel=0;

      while(paused){
        oled.clearDisplay();
        oled.setTextSize(1);oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(42,5);oled.print(F("PAUSA"));
        oled.drawLine(0,14,127,14,SSD1306_WHITE);
        for(uint8_t i=0;i<2;i++){
          if(i==pSel){
            oled.fillRect(10,20+i*14-1,108,11,SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
          } else oled.setTextColor(SSD1306_WHITE);
          oled.setCursor(14,20+i*14);
          oled.print(i==0?F("Continuar"):F("Volver al menu"));
        }
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(4,54);oled.print(F("Score:"));oled.print(gScore);
        oled.display();

        unsigned long tNow=millis();
        if(tNow-tNavP>200){
          int jyP=RJY();
          if(jyP<-DEAD){ pSel=0; tNavP=tNow; }
          if(jyP> DEAD){ pSel=1; tNavP=tNow; }
        }

        bool bNow=RBTN();
        if(bNow && btnWasUp){
          if(pSel==0){
            paused=false;
            lb=true;
          } else {
            backToMenu=true;
            paused=false;
          }
          while(RBTN()) delay(10);
          delay(80);
        }
        btnWasUp=!bNow;
        delay(30);
      }
      if(backToMenu) break;
      continue;
    }

    // Logica de juego 
    int jx=RJX(),jy=RJY();
    if(jx> DEAD) plrX=min((int16_t)(plrX+9),(int16_t) 65);
    if(jx<-DEAD) plrX=max((int16_t)(plrX-9),(int16_t)-65);
    if(jy> DEAD) plrY=max((int16_t)(plrY-7),(int16_t)-32);
    if(jy<-DEAD) plrY=min((int16_t)(plrY+7),(int16_t) 32);

    float tbx=(jx>DEAD)?0.75f:(jx<-DEAD)?-0.75f:0.0f;
    float tby=(jy>DEAD)?-0.45f:(jy<-DEAD)?0.45f:0.0f;
    bankX+=(tbx-bankX)*0.32f;
    bankY+=(tby-bankY)*0.32f;

    if(now-tShot>260){laserFire();tShot=now;}

    if(now-tSpawn>(unsigned long)spawnMs){
      asteroidSpawn();tSpawn=now;
      if(spawnMs>500)spawnMs-=12;
    }

    for(uint8_t i=0;i<N_BULL;i++){
      if(!laser[i].on)continue;
      laser[i].ez+=58;
      if(laser[i].ez>350)laser[i].on=false;
    }

    for(uint8_t i=0;i<N_ENE;i++){
      if(!ast[i].on)continue;
      ast[i].ez-=ast[i].spd;
      ast[i].ang+=ast[i].angSpd;
      if(ast[i].ez<250){
        if(ast[i].ex>0)ast[i].ex-=2;else ast[i].ex+=2;
        if(ast[i].ey>0)ast[i].ey-=1;else ast[i].ey+=1;
      }
      if(ast[i].ez<=0){
        ast[i].on=false; gLives--;
        if(gLives<=0){ gameOver=true; break; }
      }
    }
    if(gameOver) break;

    for(uint8_t b=0;b<N_BULL;b++){
      if(!laser[b].on)continue;
      for(uint8_t e=0;e<N_ENE;e++){
        if(!ast[e].on)continue;
        int16_t dz=laser[b].ez-ast[e].ez;
        if(dz<-65||dz>65)continue;
        int16_t dx=laser[b].ex-ast[e].ex-plrX;
        int16_t dy=laser[b].ey-ast[e].ey-plrY;
        if(dx>-22&&dx<22&&dy>-22&&dy<22){
          laser[b].on=false;ast[e].on=false;
          int8_t esx=GCX,esy=GCY;
          gProj(ast[e].ex,ast[e].ey,(int16_t)(ast[e].ez+CAM_DIST),&esx,&esy);
          explTrigger(esx,esy);
          gScore+=10;
        }
      }
    }

    for(uint8_t e=0;e<N_ENE;e++){
      if(!ast[e].on||ast[e].ez>50)continue;
      if(ast[e].ex>-28&&ast[e].ex<28&&ast[e].ey>-28&&ast[e].ey<28){
        ast[e].on=false;explTrigger(GCX,GCY+10);
        gLives--;
        if(gLives<=0){ gameOver=true; break; }
      }
    }
    if(gameOver) break;

    for(uint8_t i=0;i<N_EXPL;i++){
      if(!expl[i].on)continue;
      expl[i].r+=3;if(expl[i].r>22)expl[i].on=false;
    }

    oled.clearDisplay();
    starsDraw(8,plrX,plrY,GCX,GCY,GFOV,-63,63);
    for(uint8_t i=0;i<N_BULL;i++) if(laser[i].on) laserDraw(i);
    for(uint8_t i=0;i<N_ENE;i++) if(ast[i].on)
      drawAsteroid(ast[i].ex,ast[i].ey,(int16_t)(ast[i].ez+CAM_DIST),ast[i].ang,1.0f,GCX,GCY,GFOV);
    for(uint8_t i=0;i<N_EXPL;i++){
      if(!expl[i].on)continue;
      oled.drawCircle(expl[i].sx,expl[i].sy,expl[i].r,SSD1306_WHITE);
      if(expl[i].r>6)oled.drawCircle(expl[i].sx,expl[i].sy,expl[i].r-4,SSD1306_WHITE);
    }
    playerDraw();
    oled.setTextSize(1);oled.setTextColor(SSD1306_WHITE);
    for(int8_t i=0;i<gLives;i++)
      oled.fillTriangle(2+i*9,7,6+i*9,1,10+i*9,7,SSD1306_WHITE);
    oled.setCursor(88,1);oled.print(gScore);
    oled.display();
  }
  // Fin del while 
  // backToMenu=true  -> return directo, sin Game Over
  // gameOver=true    -> mostrar pantalla de Game Over
  if(backToMenu) return;

  {
    uint16_t hs=((uint16_t)EEPROM.read(EE_HI2)<<8)|EEPROM.read(EE_LO);
    if((uint16_t)gScore>hs){
      hs=(uint16_t)gScore;
      EEPROM.write(EE_LO,(uint8_t)(hs&0xFF));
      EEPROM.write(EE_HI2,(uint8_t)(hs>>8));
    }
    oled.clearDisplay();
    oled.setTextSize(2);oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(8,8);oled.print(F("GAME OVER"));
    oled.setTextSize(1);
    oled.setCursor(28,34);oled.print(F("Score: "));oled.print(gScore);
    oled.setCursor(20,46);oled.print(F("Best:  "));oled.print(hs);
    oled.display();delay(3000);
  }
}

// HIGH SCORE
static void showHighScore(){
  uint16_t hs=((uint16_t)EEPROM.read(EE_HI2)<<8)|EEPROM.read(EE_LO);
  oled.clearDisplay();
  oled.setTextSize(1);oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(28,4);oled.print(F("HIGH SCORE"));
  oled.drawLine(0,13,127,13,SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(34,24);oled.print(hs);
  oled.setTextSize(1);
  oled.setCursor(16,52);oled.print(F("Pulsa para volver"));
  oled.display();delay(400);
  while(!RBTN())delay(20);
  delay(200);
}

//SETUP Y LOOP
void setup(){
  pinMode(PIN_BTN,INPUT_PULLUP);
  randomSeed(analogRead(A3));
  if(!oled.begin(SSD1306_SWITCHCAPVCC,0x3C)){
    pinMode(LED_BUILTIN,OUTPUT);
    while(true){digitalWrite(LED_BUILTIN,HIGH);delay(300);digitalWrite(LED_BUILTIN,LOW);delay(300);}
  }
  oled.clearDisplay();oled.display();
  bootAnimation();
}

void loop(){
  uint8_t sel=runMenu();
  if(sel==0) runGame();
  else       showHighScore();
}
