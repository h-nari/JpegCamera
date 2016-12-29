#ifndef _jpeg_camera_h_
#define _jpeg_camera_h_

#include <Arduino.h>
#include <Stream.h>

class JpegCamera {
 public:
  typedef void (*cbGetSize_t)(uint32_t size);
  typedef void (*cbReadData_t)(const uint8_t *data, uint32_t dataLen);
  typedef void (*cbPicFinish_t)();
  
  enum ImageSize {
    IMAGE_SIZE_160_120,
    IMAGE_SIZE_320_240,
    IMAGE_SIZE_640_480,
    IMAGE_SIZE_800_600,
    IMAGE_SIZE_1280_960,
    IMAGE_SIZE_1600_1200,
  };
  enum CameraBaudrate {
    CB_9600,
    CB_38400,
    CB_57600,
    CB_115200,
  };
  
 protected:
  uint8_t	m_verbose;
  uint8_t	m_retryMax;
  uint16_t	m_timeout;
  uint16_t	m_w10us;
  Stream 	*m_pCom;
  
 public:
  JpegCamera(Stream *com);

  bool resetCamera();
  bool readJpegSize(uint32_t *pSize);
  bool readData(uint32_t addr, uint32_t len, uint8_t *buf);
  bool takePicture(cbGetSize_t cb1 = NULL,
		   cbReadData_t cb2 = NULL, cbPicFinish_t cb3 = NULL);
  bool stopPicture();
  bool setImageSize(ImageSize s);
  void setVerbose(uint8_t v) {m_verbose = v;}
  bool setBaudrate(CameraBaudrate b);
  
 protected:
  void readFlush(uint16_t waitMs = 0);
  bool jcError(const char *fmt, ...);
  bool sendCmdAndGetReply(const char *cmd, uint8_t cmdLen,
			  uint8_t *res,  uint32_t resLen);
  bool sendCmdAndCheckReply(const char *cmd, uint8_t cmdLen,
			    const char *res, int8_t resLen);
  bool takePictureCmd(void);
};


#endif /* _jpeg_camera_h_ */
