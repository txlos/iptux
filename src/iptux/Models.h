//
// C++ Interface: mess
//
// Description:
// 很杂乱的一些数据基本结构.
//
// Author: Jally <jallyx@163.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef IPTUX_MESS_H
#define IPTUX_MESS_H

#include <string>

#ifdef __WIN32__
#include <Winsock2.h>
#include <Ws2tcpip.h>
#undef ERROR
#undef CreateWindow
typedef int in_addr_t;
#else
//#include <arpa/inet.h>
#endif

#include <glib.h>
#include <json/json.h>


namespace iptux {

/**
 * 消息来源类型.
 */
enum class MessageSourceType {
  PAL,   ///< 好友
  SELF,  ///< 自身
  ERROR  ///< 错误
};

/**
 * 消息内容类型.
 */
typedef enum {
  MESSAGE_CONTENT_TYPE_STRING,  ///< 字符串
  MESSAGE_CONTENT_TYPE_PICTURE  ///< 图片
} MessageContentType;

/**
 * 群组所属类型
 */
typedef enum {
  GROUP_BELONG_TYPE_REGULAR,   ///< 常规
  GROUP_BELONG_TYPE_SEGMENT,   ///< 分段
  GROUP_BELONG_TYPE_GROUP,     ///< 分组
  GROUP_BELONG_TYPE_BROADCAST  ///< 广播
} GroupBelongType;

/***************偶是可爱的分割线(数据对象)*****************/

/**
 * 好友信息.
 * flags位含义: \n
 * 黑名单(:3);此IP地址被列入黑名单(deprecated) \n
 * 更改(:2);好友的信息被用户手工修改，程序不应再更改好友的信息 \n
 * 在线(:1);好友依旧在线 \n
 * 兼容(:0);完全兼容iptux，程序将采用扩展协议与好友通信 \n
 */
class PalInfo {
 public:
  PalInfo();
  ~PalInfo();

  in_addr_t ipv4;  ///< 好友IP
  char *segdes;    ///< 所在网段描述
  char *version;   ///< 版本串 *
  char *user;      ///< 好友用户 *
  char *host;      ///< 好友主机 *
  char *name;      ///< 昵称 *
  char *group;     ///< 所在群组
  char *photo;     ///< 形象照片
  char *sign;      ///< 个性签名
  char *iconfile;  ///< 好友头像 *
  char *encode;    ///< 好友编码 *
  uint32_t packetn;   ///< 已接受最大的包编号
  uint32_t rpacketn;  ///< 需要接受检查的包编号

  bool isCompatible() const;
  bool isOnline() const;
  bool isChanged() const;
  bool isInBlacklist() const;

  void setCompatible(bool value);
  void setOnline(bool value);
  void setChanged(bool value);
  void setInBlacklistl(bool value);

 private:
  uint8_t flags;   ///< 3 黑名单:2 更改:1 在线:0 兼容
};

/**
 * 文件信息.
 */
class FileInfo {
 public:
  FileInfo();
  ~FileInfo();

  uint32_t fileid;       ///< 唯一标识
  uint32_t packetn;      ///< 包编号
  uint32_t fileattr;     ///< 文件属性
  int64_t filesize;      ///< 文件大小
  int64_t finishedsize;  ///< 已完成大小
  PalInfo *fileown;      ///< 文件拥有者(来自好友*)
  char *filepath;        ///< 文件路径 *
  uint32_t filectime;    ///<  文件创建时间
  uint32_t filemtime;    ///<  文件最后修改时间
  uint32_t filenum;      ///<  包内编号
};

/**
 * 碎片数据.
 */
class ChipData {
 public:
  ChipData();
  ~ChipData();

  MessageContentType type;  ///< 消息内容类型
  std::string data;               ///< 数据串 *
};

/**
 * 消息参数.
 */
class MsgPara {
 public:
  MsgPara();
  ~MsgPara();

  PalInfo *pal;             ///< 好友数据信息(来自好友*)
  MessageSourceType stype;  ///< 来源类型
  GroupBelongType btype;    ///< 所属类型
  std::vector<ChipData> dtlist;           ///< 数据链表 *
};

/**
 * 网段数据.
 */
class NetSegment {
 public:
  NetSegment();
  ~NetSegment();

  bool ContainIP(in_addr_t ipv4) const;

  std::string startip;      ///< IP起始地址 *
  std::string endip;        ///< IP终止地址 *
  std::string description;  ///< 此IP段描述

  Json::Value ToJsonValue() const;
  static NetSegment fromJsonValue(Json::Value &value);
};

/***************偶是可爱的分割线(抽象类)*****************/

/**
 * 会话抽象类.
 * 提供好友会话类必需的公共接口.
 */
class SessionAbstract {
 public:
  SessionAbstract();
  virtual ~SessionAbstract();

  virtual void UpdatePalData(PalInfo *pal) = 0;  ///< 更新好友数据
  virtual void InsertPalData(PalInfo *pal) = 0;  ///< 插入好友数据
  virtual void DelPalData(PalInfo *pal) = 0;     ///< 删除好友数据
  virtual void ClearAllPalData() = 0;            ///< 清除所有好友数据
  //        virtual void ShowEnclosure() = 0;               ///< 显示附件
  virtual void AttachEnclosure(const GSList *list) = 0;  ///< 添加附件
  virtual void OnNewMessageComing() = 0;  ///< 窗口打开情况下有新消息
};

}  // namespace iptux

#endif
