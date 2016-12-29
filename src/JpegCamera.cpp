#include "JpegCamera.h"

JpegCamera::JpegCamera(Stream *com)
{
  Serial.begin(115200);
  m_pCom = com;
  readFlush();
  m_verbose = 0;
  m_retryMax = 5;
  m_timeout = 1000;
  m_w10us = 10;
}

void JpegCamera::readFlush(uint16_t waitMs)
{
  unsigned long tStart = millis();
  while(1){
    while(m_pCom->available()){
      uint8_t c = m_pCom->read();
      if(m_verbose && Serial)
	Serial.printf("flush:%02x\n",c);
      tStart = millis();
    }
    if(millis() - tStart > waitMs)
      break;
    delay(0);
  }
}

bool JpegCamera::jcError(const char *fmt, ...)
{
  va_list ap;
  char err[80];
  
  Serial.printf("%s:%d\n",__FUNCTION__,__LINE__);

  readFlush(10);
  va_start(ap,fmt);
  vsnprintf(err,sizeof err, fmt, ap);
  va_end(ap);
  Serial.println(err);
  return false;
}

bool JpegCamera::sendCmdAndGetReply(const char *cmd, uint8_t cmdLen,
				    uint8_t *res,  uint32_t resLen)
{
  if(cmd && cmdLen > 0){
    readFlush();
    m_pCom->write(cmd, cmdLen);
  }
  delay(0);
  unsigned long tStart = millis();
  int len = 0;
  while(1){
    while(m_pCom->available()> 0){
      res[len++] = m_pCom->read();
      if(len >= resLen)
	return true;
    }
    if(millis() - tStart > m_timeout)
      return jcError("cmd 0x%02x timeout",cmd[2]);
    delay(0);
  }
}

bool JpegCamera::sendCmdAndCheckReply(const char *cmd, uint8_t cmdLen,
				      const char *res, int8_t resLen)
{
  uint8_t buf[8];
  
  for(int rc=0; rc < m_retryMax; rc++){
    if(resLen > sizeof buf)
      return jcError("resLen exceeded");
    if(!sendCmdAndGetReply(cmd, cmdLen, buf, resLen))
      continue;
    if(memcmp(buf, res, resLen)==0)
      return true;
    if(m_verbose) {
      Serial.printf("cmd 0x%02x bad response.\n",cmd[2]);
      for(int i=0;i<resLen;i++)
	Serial.printf("%02x ",res[i] & 0xff);
      Serial.printf("expected. ");
      for(int i=0;i<resLen;i++)
	Serial.printf("%02x ",buf[i]);
      Serial.printf("recieved.\n");
    }
  }
  return jcError("cmd 0x%02x retry failed",cmd[2]);
}


bool JpegCamera::resetCamera(void)
{
  const char *pat = "625\r\nInit end\r\n";
  const uint8_t patLen = 15;

  for(int rc=0; rc < m_retryMax; rc++){
    readFlush();
    m_pCom->write("\x56\x00\x26\x00",4);
    unsigned long tStart = millis();

    int i = 0;
    while(1){
      while(m_pCom->available()>0){
	if(i >= patLen)
	  return true;
	char c = m_pCom->read();
	if(c == pat[i])
	  i++;
	else
	  i = 0;
      }
      if(millis() - tStart > m_timeout)
	break;
      delay(0);
    }
  }
  return jcError("reset cmd retry exceeded");
}

bool JpegCamera::readJpegSize(uint32_t *pSize)
{
  uint8_t buf[9];

  for(int rc=0; rc < m_retryMax; rc++){
    if(!sendCmdAndGetReply("\x56\x00\x34\x01\x00",5, buf, 9))
      continue;
    if(memcmp(buf,"\x76\x00\x34\x00\x04", 5) == 0) {
      int32_t s = buf[5] << 24;
      s |= buf[6] << 16;
      s |= buf[7] <<  8;
      s |= buf[8];
      if(pSize) *pSize = s;
      return true;
    }
  }
  return jcError("readJpegSize cmd retry failed");    
}

bool JpegCamera::takePictureCmd(void)
{
  return sendCmdAndCheckReply("\x56\x00\x36\x01\x00", 5,
			      "\x76\x00\x36\x00\x00", 5);
}


static const uint8_t size_code[7] = {0x22,0x11,0x00,0x1d,0x1c,0x1b,0x21}; 

bool JpegCamera::setImageSize(ImageSize s)
{
  uint8_t c = 0;
  if(s >= 0 && s < sizeof(size_code))
    c = size_code[s];

  char cmd[5];
  memcpy(cmd,"\x56\x00\x54\x01",4);
  cmd[4] = c;
  
  return sendCmdAndCheckReply(cmd, 5, "\x76\x00\x54\x00\x00", 5);
}

static const uint16_t baudrate_code[] = {0xaec8,0x2af2,0x1c1c,0x0da6};

bool JpegCamera::setBaudrate(CameraBaudrate b)
{
  uint8_t c = 2;
  if(b >= 0 && b < sizeof(baudrate_code))
    c = baudrate_code[b];
  char cmd[7];
  memcpy(cmd,"\x56\x00\x24\x03\x01",5);
  cmd[5] = c >> 8;
  cmd[6] = c;
  return sendCmdAndCheckReply(cmd, 7, "\x76\x00\x24\x00\x00", 5);
}

bool JpegCamera::readData(uint32_t addr, uint32_t length, uint8_t *buf)
{
  char cmd[16];

  if(m_verbose)
    Serial.printf("readData addr:%u len:%u\n",addr, length);
  memcpy(cmd,"\x56\x00\x32\x0c\x00\x0a",6);
  cmd[6]  = addr >> 24;
  cmd[7]  = addr >> 16;
  cmd[8]  = addr >>  8;
  cmd[9]  = addr;
  cmd[10] = length >> 24;
  cmd[11] = length >> 16;
  cmd[12] = length >>  8;
  cmd[13] = length;
  cmd[14] = m_w10us >> 8;
  cmd[15] = m_w10us;

  for(int rc=0; rc < m_retryMax; rc++){
    if(!sendCmdAndCheckReply(cmd, 16, "\x76\x00\x32\x00\x00", 5))
      return jcError("readData failed(1)");
    if(!sendCmdAndGetReply(NULL, 0, buf, length))
      continue;
    uint8_t dummy[5];
    if(!sendCmdAndGetReply(NULL, 0, dummy, 5))
      continue;
    return true;
  }
  return jcError("readData failed(2)");
}

bool JpegCamera::stopPicture(void)
{
  sendCmdAndCheckReply("\x56\x00\x36\x01\x03",5,"\x76\x00\x36\x00\x00",5);
}


bool JpegCamera::takePicture(cbGetSize_t cb1,
			     cbReadData_t cb2, cbPicFinish_t cb3)
{
  uint32_t fileSize, addr;
  
  if(!takePictureCmd())
    return false;

  if(!readJpegSize(&fileSize))
    return false;

  if(cb1)
    (*cb1)(fileSize);

  uint8_t buf[256];
  for(addr = 0; addr < fileSize; addr += sizeof buf){
    uint32_t len = fileSize - addr;
    if(len > sizeof buf)
      len = sizeof buf;
    if(len & 7) len = (len & ~7) + 8;
    if(!readData(addr, len, buf))
      return false;
    if(cb2)
      (*cb2)(buf, len);
  }
  if(cb3)
    (*cb3)();
  return stopPicture();
}

