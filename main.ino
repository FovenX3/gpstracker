#include <U8g2lib.h>
#include <SD.h>
#include <TinyGPS.h>
#include <Wire.h>
#include <SPI.h>

TinyGPS gps;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE); 
//u8g2LIB_PCD8544 u8g2(3, 4, 99, 5, 6);  // SPI Com: SCK = 3, MOSI = 4, CS = 永远接地, A0 = 5, Reset = 6
File myFile;
boolean sderror = false;                //TF卡状态
char logname[13];                            //记录文件名
boolean writelog = true;                //是否要记录当前数据到TF卡标志
boolean refresh = false;                //是否要更新液晶显示标志
boolean finish_init = false;    //初始化完成标志
byte satnum=0;          //连接上的卫星个数
float flat, flon, spd, alt, oflat, oflon;       //GPS信息，经纬度、速度、高度、上一次的经纬度
unsigned long age;                        //GPS信息 fix age
int year;                                          //GPS信息 年
byte month, day, hour, minute, second, hundredths;            //GPS信息 时间信息
char crs[4];                    //GPS信息 行驶方向
char sz[10];                    //文本信息
byte cnt=0;                              //循环计数器
static void gpsdump(TinyGPS &gps);
static bool feedgps();
static void print_date(TinyGPS &gps);
static void print_satnum(TinyGPS &gps);
static void print_pos(TinyGPS &gps);
static void print_alt(TinyGPS &gps);
static void print_speed(TinyGPS &gps);
static String float2str(float val, byte len);
//AVR定时器，每秒触发
ISR(TIMER1_OVF_vect) {
 TCNT1=0x0BDC; // set initial value to remove time error (16bit counter register)
 if (finish_init) refresh = true;      //在完成初始化后，将刷新显示标志设为true
}
void setup()
{
 finish_init = false;
 //设置并激活AVR计时器
 TIMSK1=0x01; // 启用全局计时器中断
 TCCR1A = 0x00; //normal operation page 148 (mode0);
 TCNT1=0x0BDC; //set initial value to remove time error (16bit counter register)
 TCCR1B = 0x04; //启动计时器
 pinMode(10, OUTPUT);
 oflat = 0;
 oflon = 0;
 logname[0]=' ';

 //Serial1.begin(9600); //GPS模块默认输出9600bps的NMEA信号
 Serial3.begin(9600);
 //u8g2.setColorIndex(1);         // 设置LCD显示模式，黑白
 u8g2.begin();
 u8g2.setFont(u8g2_font_8x13B_mr);       //字体
 //TF卡的片选端口是10
 if (!SD.begin(10)) {
   sderror = true;
 }
 //初始化完成
 finish_init = true;
}
void loop()
{
  //读取并分析GPS数据
  feedgps();
  //刷新显示
  if (refresh)
  {
     cnt %=10;
     writelog = true;
 
     u8g2.firstPage();
     do{
       gpsdump(gps);
       u8g2.setCursor(122,64);
       u8g2.print( cnt);  
     } while ( u8g2.nextPage() );  
   
         //每5秒且GPS信号正常时将数据记录到TF卡
     if (cnt % 5 == 0 && writelog)
     {
       logEvent();
     }
         //刷新完毕，更新秒计数器
     refresh = false;
     cnt++;
  }
}
static void gpsdump(TinyGPS &gps)
{
 print_satnum(gps);
 print_date(gps);
 print_pos(gps);
 print_speed(gps);
 print_alt(gps);

}
//更新并显示卫星个数
static void print_satnum(TinyGPS &gps)
{
 satnum = gps.satellites();
 if ( satnum != TinyGPS::GPS_INVALID_SATELLITES){
   u8g2.setFont(u8g2_font_lucasfont_alternate_tf);
   u8g2.drawStr(70, 10, "SAT :");
   u8g2.setCursor( 100, 10);
   u8g2.print(satnum);
   writelog &= true;
 }
 else {
   u8g2.setFont(u8g2_font_t0_12_me);
   
   u8g2.drawStr( 5, 11, ("GPS Tracking System"));
   u8g2.drawStr( 16, 23, (" WWW.HYCDGX.COM"));
   u8g2.drawStr( 5, 44, (cnt % 2) ? ("Searching satellite") : ("            "));
   writelog = false;
 }
 feedgps();
}
//更新并显示GPS经纬度信息
static void print_pos(TinyGPS &gps)
{
 gps.f_get_position(&flon, &flat, &age);
 if (flat != TinyGPS::GPS_INVALID_F_ANGLE && flon !=  TinyGPS::GPS_INVALID_F_ANGLE) {
   u8g2.setCursor(0,50);
   u8g2.setFont(u8g2_font_crox1tb_tr);
   u8g2.print(float2str(flon,8));
   u8g2.print(F(" : "));
   u8g2.print(float2str(flat,8));
   writelog &= true;
 }
 else
   writelog = false;
 feedgps();
}
//更新并显示GPS高度信息
static void print_alt(TinyGPS &gps)
{
 alt = gps.f_altitude();
 if (alt != TinyGPS::GPS_INVALID_F_ALTITUDE){
   u8g2.setCursor(0,64);
   u8g2.setFont(u8g2_font_lucasfont_alternate_tf);
   u8g2.print(F("ALT: "));
   u8g2.print(float2str(alt,5));
   writelog &= true;
 }
 else
 {
   writelog = false;
 }
 feedgps();
}

//更新并显示GPS时间
static void print_date(TinyGPS &gps)
{
 gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
 if (age != TinyGPS::GPS_INVALID_AGE && month>0 && day>0)
 {
   u8g2.setFont(u8g2_font_lucasfont_alternate_tf);
   u8g2.drawStr(0, 10, "Time");
   u8g2.setCursor( 29, 10);
   sprintf(sz, "%02d",((hour+19) % 24));
   u8g2.print(sz);
   u8g2.setCursor( 43, 10);
   u8g2.print(second % 2 ? F(":") : F(" "));
   u8g2.setCursor(45,10);
   sprintf(sz, "%02d",minute);
   u8g2.print(sz);
   writelog &= true;
 }
 else
   writelog = false;
 feedgps();
}
//更新并显示时速
static void print_speed(TinyGPS &gps)
{
 spd = gps.f_speed_kmph();
 if (spd != TinyGPS::GPS_INVALID_F_SPEED)
 {
       if(spd<0.5) spd=0.0; //屏蔽0时速时的误差显示
   u8g2.setFont(u8g2_font_lucasfont_alternate_tf);  
   u8g2.drawStr(0,21, ("SPEED"));
   u8g2.setCursor(37,32);
   u8g2.setFont(u8g2_font_crox4tb_tr); //使用大字体
   u8g2.print( float2str(spd,5));
   u8g2.setFont(u8g2_font_crox4tb_tr);
   u8g2.print(F(" km/h"));
   writelog &= true;
 }
 else
   writelog = false;
 feedgps();
}
//读取并解码GPS信息
static bool feedgps()
{
 while (Serial3.available())
 {
   if (gps.encode(Serial3.read()))
     return true;
 }
 return false;
}
//将浮点数转化为字符串（整数部分<1000）
static String float2str(float val, byte len)
{
 String str = "";
 char tmp[4];
 byte pos = 0;
 int p1;
 bool minus=false;
 //取绝对值
 if (val<0)
 {
   minus=true;
   len--;
   val = abs(val);
 }
 p1=(int)val;  //取整数部分
 val=val-p1;   //得到小数部分
 itoa(p1,tmp,10);      //整数部分转化为字符串
 str.concat(tmp);
 //获得小数点位置
 if (p1 == 0) {
   pos = 1;
 }
 else {
   for (pos=0;pos<len && p1>0;pos++)
     p1=p1/10;
 }
 //小数点
 if (pos<len && val>0){
   pos++;
   str.concat('.');
 }
 //小数部分加入字符串
 for (;pos<len&& val>0;pos++)
 {
   str.concat((char)('0'+ (byte)(val*10)));
   val= val * 10 - ((byte)(val*10));
 }
 if (minus) str = "-" + str;
 return str;
}
//记录数据到TF卡
void logEvent()
{
 if (logname[0]==' ') {        //获取文件名
   sprintf(logname, "%04d%02d%02d.trc",  year, month, day, hour, minute, second);
   myFile=SD.open(logname, FILE_WRITE);
   if (!myFile) {
     sderror = true;
   }
   else {
       myFile.print("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
       myFile.print("<gpx version=\"1.0\">\n");
       myFile.print("<rte>\n");
       myFile.print("<name>Line1</name>\n");
      myFile.close();
      delay(10);
   }
 }
 //当位置和上一次（5秒前）相比发生一定变化量时才记录数据（节省数据文件的空间）
 if (writelog && logname[0]!=' ' &&  (abs(flat-oflat) > 0.0001 || abs(flon - oflon) > 0.0001))
 {
   myFile=SD.open(logname, FILE_WRITE);
   if (!myFile) {
     sderror = true;
   }
   else
   {
     oflat = flat;
     oflon = flon;
 
     
     myFile.print("<rtept lat=\"");
     myFile.print(float2str(flon,20));
     myFile.print("\" lon=\"");
     myFile.print(float2str(flat,20));
     myFile.print("\">\n");
     myFile.print("<ele>");
     myFile.print(float2str(alt,10));
     myFile.print("</ele>\n");
     myFile.print("<time>");
     sprintf(sz, "%04d-%02d-%02d",  year, month, day);
     myFile.print(sz);
     myFile.print("T");
     sprintf(sz, "%02d:%02d:%02d", hour, minute, second);
     myFile.print(sz);
     myFile.print("Z");
     myFile.print("</time>\n");
     myFile.print("</rtept>\n");
     
    // sprintf(sz, "%04d-%02d-%02d",  year, month, day);
     //myFile.print(sz);
     //myFile.print(F(","));
//     sprintf(sz, "%02d:%02d:%02d,", hour, minute, second);
//     myFile.print(sz);
//     sprintf(sz,"%02d,",satnum);
//     myFile.print(sz);
//     myFile.print(float2str(flat,20));
//     myFile.print(",");
//     myFile.print(float2str(flon,20));
//     myFile.print(",");
//     myFile.print(float2str(alt,10));
//     myFile.print(",");
//     myFile.print(float2str(spd,10));
//     myFile.print(",");
     delay(20);
     myFile.flush();
     delay(50);
     myFile.close();
   
   }
 }
}
