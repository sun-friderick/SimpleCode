/*
获取 MP3 中的 ID3V1、ID3V2 信息 
近日公司分配给我任务，其中一个就是读取MP3文件的TAG信息。经过在网上搜索，自己总结出来比较完整的读取代码。
什么是ID3信息？这个我不做回答，请自己访问 www.id3.org（E文网站，E文不好这自己使用 www.google.com 搜索http://www.google.com/search?hl=zh-CN&q=%E4%BB%80%E4%B9%88%E6%98%AFID3&lr=lang_zh-CN 
 
本来自己想使用 VC6 写一个示例工程的，但是发觉自己现在越来越懒了，不写了，仅仅把C代码贴出来，说明一下就算了。
 
1、  贴出来的是两个文件.h和.c，注意：这并不是一个类，而是在C中使用的文件
2、  代码中读文件使用的是API函数，例如HFILE, ReadFile(..)等，这也是我太懒了，不愿意改过来 J
3、  代码很简单，我也不做详细说明了，自己慢慢看吧
*/
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  .h 文件
/********************************************************************
  文件说明: 读取Audio文件中的信息。例如读入MP3中的ID3V1信息
  文件名称: audioInfo.h
  版本号  : 1.0.0
  开发日期:       2005-10-24   9:11
  作    者: hengai
  修改纪录: 
      2005-10-24 17:30: 扩充了 ID3V2 的TAG定义。并且由原来的 #define 更改为 static LPCTSTR
      2005-10-24 18:10: 能够读取 MP3 中的所有ID3V2信息。
          更改了读取的结构体 MP3ID3V2INFO 定义以及用户使用这些信息的方式(在结构体定义中有说明)
*********************************************************************/ 
 
#define AUDIO_OUT_ERRMSG        //定义能够输出出错信息
//////////////////////////////////////////////////////////////////////////
//定义读取 MP3 ID3V1 所需要的结构体
//MP3 ID3V1 信息保存到MP3文件的最后128个字节中
typedef struct tag_MP3ID3V1INFO      //MP3信息的结构ShitMP3v1.07
{
  TCHAR   Identify[3];     //TAG三个字母。这里可以用来鉴别是不是文件信息内容
  TCHAR   Title[30];       //歌曲名，30个字节
  TCHAR   Artist[30];      //歌手名，30个字节
  TCHAR   Album[30];       //所属唱片，30个字节
  TCHAR   Year[4];         //年，4个字节
  TCHAR   Comment[28];     //注释，一般为28个字节（也可能30个字节，这时候占用下面的2个字节）
  UCHAR   CommentFlag;     //保留位1，注释长度标志位。如果是 0x00 表明注释长度为28，否则为30
  UCHAR   Track;           //保留位2，那个“第几首”，是个整数
  UCHAR   Genre;           //保留位3，歌曲风格，在0－148之间。例如Pop，General...
  TCHAR   pszGenre[30];    //保存了音乐的类型。
 //
 BOOL   bHasTag;         //这个 MP3 是否存在 ID3V1 TAG信息
} MP3ID3V1INFO, *PMP3ID3V1INFO;
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//MP3 ID3V2 FrameID
enum ID3_FrameID{
  /* ???? */ ID3FID_NOFRAME = 0,       /**< No known frame */
  /* AENC */ ID3FID_AUDIOCRYPTO,       /**< Audio encryption */
  /* APIC */ ID3FID_PICTURE,           /**< Attached picture */
  /* ASPI */ ID3FID_AUDIOSEEKPOINT,    /**< Audio seek point index */
  /* COMM */ ID3FID_COMMENT,           /**< Comments */
  /* COMR */ ID3FID_COMMERCIAL,        /**< Commercial frame */
  /* ENCR */ ID3FID_CRYPTOREG,         /**< Encryption method registration */
  /* EQU2 */ ID3FID_EQUALIZATION2,     /**< Equalisation (2) */
  /* EQUA */ ID3FID_EQUALIZATION,      /**< Equalization */
  /* ETCO */ ID3FID_EVENTTIMING,       /**< Event timing codes */
  /* GEOB */ ID3FID_GENERALOBJECT,     /**< General encapsulated object */
  /* GRID */ ID3FID_GROUPINGREG,       /**< Group identification registration */
  /* IPLS */ ID3FID_INVOLVEDPEOPLE,    /**< Involved people list */
  /* LINK */ ID3FID_LINKEDINFO,        /**< Linked information */
  /* MCDI */ ID3FID_CDID,              /**< Music CD identifier */
  /* MLLT */ ID3FID_MPEGLOOKUP,        /**< MPEG location lookup table */
  /* OWNE */ ID3FID_OWNERSHIP,         /**< Ownership frame */
  /* PRIV */ ID3FID_PRIVATE,           /**< Private frame */
  /* PCNT */ ID3FID_PLAYCOUNTER,       /**< Play counter */
  /* POPM */ ID3FID_POPULARIMETER,     /**< Popularimeter */
  /* POSS */ ID3FID_POSITIONSYNC,      /**< Position synchronisation frame */
  /* RBUF */ ID3FID_BUFFERSIZE,        /**< Recommended buffer size */
  /* RVA2 */ ID3FID_VOLUMEADJ2,        /**< Relative volume adjustment (2) */
  /* RVAD */ ID3FID_VOLUMEADJ,         /**< Relative volume adjustment */
  /* RVRB */ ID3FID_REVERB,            /**< Reverb */
  /* SEEK */ ID3FID_SEEKFRAME,         /**< Seek frame */
  /* SIGN */ ID3FID_SIGNATURE,         /**< Signature frame */
  /* SYLT */ ID3FID_SYNCEDLYRICS,      /**< Synchronized lyric/text */
  /* SYTC */ ID3FID_SYNCEDTEMPO,       /**< Synchronized tempo codes */
  /* TALB */ ID3FID_ALBUM,             /**< Album/Movie/Show title */
  /* TBPM */ ID3FID_BPM,               /**< BPM (beats per minute) */
  /* TCOM */ ID3FID_COMPOSER,          /**< Composer */
  /* TCON */ ID3FID_CONTENTTYPE,       /**< Content type */
  /* TCOP */ ID3FID_COPYRIGHT,         /**< Copyright message */
  /* TDAT */ ID3FID_DATE,              /**< Date */
  /* TDEN */ ID3FID_ENCODINGTIME,      /**< Encoding time */
  /* TDLY */ ID3FID_PLAYLISTDELAY,     /**< Playlist delay */
  /* TDOR */ ID3FID_ORIGRELEASETIME,   /**< Original release time */
  /* TDRC */ ID3FID_RECORDINGTIME,     /**< Recording time */
  /* TDRL */ ID3FID_RELEASETIME,       /**< Release time */
  /* TDTG */ ID3FID_TAGGINGTIME,       /**< Tagging time */
  /* TIPL */ ID3FID_INVOLVEDPEOPLE2,   /**< Involved people list */
  /* TENC */ ID3FID_ENCODEDBY,         /**< Encoded by */
  /* TEXT */ ID3FID_LYRICIST,          /**< Lyricist/Text writer */
  /* TFLT */ ID3FID_FILETYPE,          /**< File type */
  /* TIME */ ID3FID_TIME,              /**< Time */
  /* TIT1 */ ID3FID_CONTENTGROUP,      /**< Content group description */
  /* TIT2 */ ID3FID_TITLE,             /**< Title/songname/content description */
  /* TIT3 */ ID3FID_SUBTITLE,          /**< Subtitle/Description refinement */
  /* TKEY */ ID3FID_INITIALKEY,        /**< Initial key */
  /* TLAN */ ID3FID_LANGUAGE,          /**< Language(s) */
  /* TLEN */ ID3FID_SONGLEN,           /**< Length */
  /* TMCL */ ID3FID_MUSICIANCREDITLIST,/**< Musician credits list */
  /* TMED */ ID3FID_MEDIATYPE,         /**< Media type */
  /* TMOO */ ID3FID_MOOD,              /**< Mood */
  /* TOAL */ ID3FID_ORIGALBUM,         /**< Original album/movie/show title */
  /* TOFN */ ID3FID_ORIGFILENAME,      /**< Original filename */
  /* TOLY */ ID3FID_ORIGLYRICIST,      /**< Original lyricist(s)/text writer(s) */
  /* TOPE */ ID3FID_ORIGARTIST,        /**< Original artist(s)/performer(s) */
  /* TORY */ ID3FID_ORIGYEAR,          /**< Original release year */
  /* TOWN */ ID3FID_FILEOWNER,         /**< File owner/licensee */
  /* TPE1 */ ID3FID_LEADARTIST,        /**< Lead performer(s)/Soloist(s) */
  /* TPE2 */ ID3FID_BAND,              /**< Band/orchestra/accompaniment */
  /* TPE3 */ ID3FID_CONDUCTOR,         /**< Conductor/performer refinement */
  /* TPE4 */ ID3FID_MIXARTIST,         /**< Interpreted, remixed, or otherwise modified by */
  /* TPOS */ ID3FID_PARTINSET,         /**< Part of a set */
  /* TPRO */ ID3FID_PRODUCEDNOTICE,    /**< Produced notice */
  /* TPUB */ ID3FID_PUBLISHER,         /**< Publisher */
  /* TRCK */ ID3FID_TRACKNUM,          /**< Track number/Position in set */
  /* TRDA */ ID3FID_RECORDINGDATES,    /**< Recording dates */
  /* TRSN */ ID3FID_NETRADIOSTATION,   /**< Internet radio station name */
  /* TRSO */ ID3FID_NETRADIOOWNER,     /**< Internet radio station owner */
  /* TSIZ */ ID3FID_SIZE,              /**< Size */
  /* TSOA */ ID3FID_ALBUMSORTORDER,    /**< Album sort order */
  /* TSOP */ ID3FID_PERFORMERSORTORDER,/**< Performer sort order */
  /* TSOT */ ID3FID_TITLESORTORDER,    /**< Title sort order */
  /* TSRC */ ID3FID_ISRC,              /**< ISRC (international standard recording code) */
  /* TSSE */ ID3FID_ENCODERSETTINGS,   /**< Software/Hardware and settings used for encoding */
  /* TSST */ ID3FID_SETSUBTITLE,       /**< Set subtitle */
  /* TXXX */ ID3FID_USERTEXT,          /**< User defined text information */
  /* TYER */ ID3FID_YEAR,              /**< Year */
  /* UFID */ ID3FID_UNIQUEFILEID,      /**< Unique file identifier */
  /* USER */ ID3FID_TERMSOFUSE,        /**< Terms of use */
  /* USLT */ ID3FID_UNSYNCEDLYRICS,    /**< Unsynchronized lyric/text transcription */
  /* WCOM */ ID3FID_WWWCOMMERCIALINFO, /**< Commercial information */
  /* WCOP */ ID3FID_WWWCOPYRIGHT,      /**< Copyright/Legal infromation */
  /* WOAF */ ID3FID_WWWAUDIOFILE,      /**< Official audio file webpage */
  /* WOAR */ ID3FID_WWWARTIST,         /**< Official artist/performer webpage */
  /* WOAS */ ID3FID_WWWAUDIOSOURCE,    /**< Official audio source webpage */
  /* WORS */ ID3FID_WWWRADIOPAGE,      /**< Official internet radio station homepage */
  /* WPAY */ ID3FID_WWWPAYMENT,        /**< Payment */
  /* WPUB */ ID3FID_WWWPUBLISHER,      /**< Official publisher webpage */
  /* WXXX */ ID3FID_WWWUSER,           /**< User defined URL link */
  /*      */ ID3FID_METACRYPTO,        /**< Encrypted meta frame (id3v2.2.x) */
  /*      */ ID3FID_METACOMPRESSION,   /**< Compressed meta frame (id3v2.2.1) */
  /* >>>> */ ID3FID_LASTFRAMEID,       /**< Last field placeholder */
  ID3FID_MAX_COUNT,
};
////定义读取 MP3 ID3V2 所需要的结构体
typedef struct tag_MP3ID3V2INFO
{
  TCHAR   Identify[3];    //ID3三个字母。这里可以用来鉴别是不是文件信息内容
  struct
  {
    UCHAR majorversion;   //主版本号
    UCHAR revision;       //次版本号
  }       Version;
  UCHAR   HeaderFlags;
  UCHAR   HeaderSize[4];  //Note:每byte数值不能大于0x80,即该size值的每个bit 7均为0
  //上面的信息，共 10 个字节，成为 ID3V2 的头信息
  BOOL    bHasExtHeader;  //该 ID3V2 信息中是否含有扩展头部
  BOOL    bHasExtFooter;  //该 ID3V2 信息中是否含有扩展尾部
  DWORD   dwInfoSize;     //ID3V2 信息的大小
  LPSTR   ppszTagInfo[ID3FID_MAX_COUNT];  //求出来的 Tag 信息。如果存在则ppszTagInfo[]!=NULL
      //注意: 如果是要 Comment 信息，那么请从第4字节读起，也就是使用LPCSTR pszComment = (LPCTSTR)&ppszTagInfo[ID3FID_COMMENT][4];
      //备注是以：eng开始的，所以应该向后搓4个字节
  BOOL    bHasTag;        //这个 MP3 中是否包含 ID3V2 信息
}MP3ID3V2INFO, *PMP3ID3V2INFO;
 
//////////////////////////////////////////////////////////////////////////
#ifndef STATIC
#define STATIC  static
#endif
////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//              下面定义外部接口函数
//
//////////////////////////////////////////////////////////////////////////
LPCTSTR Audio_GetErrorString();                       //获取出错信息
 
/*********************************************************************
  函数声明: int AudioInfo_GetMP3ID3V1(HFILE hAudioFile, MP3ID3V1INFO *pMp3ID3V1Info)
  参    数:
          IN: HFILE hMP3File: 需要读取信息的 MP3 文件
              MP3ID3V1INFO *pMp3ID3V1Info: 读取了 MP3  信息的返回结构体缓冲
         OUT: 
         I/O: 
    返回值: 成功返回>=0，失败返回<0。这时可以使用函数 Audio_GetErrorString 来获取失败信息 
            如果该 MP3 文件中存在 ID3V1，那么 bHasTag == TRUE
  功能描述: 读取 MP3 中的 ID3V1 信息
  引    用: 
*********************************************************************/
int AudioInfo_GetMP3ID3V1(HFILE hMP3File, MP3ID3V1INFO *pMP3ID3V1Info);
 
/*********************************************************************
  函数声明: int AudioInfo_GetMP3ID3V2(HFILE hMP3File, MP3ID3V2INFO *pMP3ID3V2Info)
  参    数:
          IN: HFILE hMP3File: 需要读取信息的 MP3 文件
              MP3ID3V2INFO *pMp3ID3V2Info: 读取了 MP3  信息的返回结构体缓冲
         OUT: 
         I/O: 
    返回值: 成功返回>=0，失败返回<0。这时可以使用函数 Audio_GetErrorString 来获取失败信息
            如果该 MP3 文件中存在 ID3V2，那么 bHasTag == TRUE
  功能描述: 读取 MP3 中的 ID3V2 信息
      注意: 使用ID3V2信息完毕后，请调用函数 AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info)
  引    用: 
*********************************************************************/
int AudioInfo_GetMP3ID3V2(HFILE hMP3File, MP3ID3V2INFO *pMP3ID3V2Info);
 
/*********************************************************************
  函数声明: VOID AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info)
  参    数:
          IN: MP3ID3V2INFO *pMP3ID3V2Info: 需要清空的结构体
         OUT: 
         I/O: 
    返回值: 
  功能描述: 清空 MP3ID3V2INFO 中的信息，回收内存
  引    用: 
*********************************************************************/
VOID AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info);
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  .c 文件
/********************************************************************
  文件说明: 读取Audio文件中的信息。例如读入MP3中的ID3V1信息
  文件名称: audioInfo.c
  版本号  : 1.0.0
  开发日期:   2005-10-24   9:11
  作    者: hengai
  修改纪录: 
  修改纪录: 
      2005-10-24 17:30: 扩充了 ID3V2 的TAG定义。并且由原来的 #define 更改为 static LPCTSTR
      2005-10-24 18:10: 能够读取 MP3 中的所有ID3V2信息。
          更改了读取的结构体 MP3ID3V2INFO 定义以及用户使用这些信息的方式(在结构体定义中有说明)
        修改了函数 AudioInfo_GetFrameHeader(..)使之能够完全读取 ID3V2 信息
        修改了函数 AudioInfo_MP3ID3V2Free(..)使之能够完全释放内存
*********************************************************************/
#include "audioInfo.h"
 
#ifdef AUDIO_OUT_ERRMSG
STATIC TCHAR m_szLastError[256];  //这里保存了出错的信息
#define AUDIO_ERR_OUT(str_err) strcpy(m_szLastError,str_err);
LPCTSTR Audio_GetErrorString()
{
  return (LPCTSTR)m_szLastError;
}
#else
#define AUDIO_ERR_OUT
LPCTSTR Audio_GetErrorString()
{
  return "Plese #define AUDIO_OUT_ERRMSG in audioInfo.h";
}
#endif
//定义 MP3 文件的音乐类型
STATIC int m_nMP3ID3v2GenreCount = 148;
STATIC LPCTSTR m_arrMP3ID3v2Genre[] = {
    "Blues","Classic Rock","Country","Dance","Disco","Funk","Grunge","Hip-Hop",
    "Jazz","Metal","New Age","Oldies","Other","Pop","R&B","Rap","Reggae","Rock",
    "Techno","Industrial","Alternative","Ska","Death Metal","Pranks","Soundtrack",
    "Euro-Techno","Ambient","Trip Hop","Vocal","Jazz Funk","Fusion","Trance",
    "Classical","Instrumental","Acid","House","Game","Sound Clip","Gospel","Noise",
    "Alternative Rock","Bass","Soul","Punk","Space","Meditative","Instrumental Pop",
    "Instrumental Rock","Ethnic","Gothic","Darkwave","Techno-Industrial","Electronic",
    "Pop Folk","Eurodance","Dream","Southern Rock","Comedy","Cult","Gangsta","Top 40",
    "Christian Rap","Pop Funk","Jungle","Native American","Cabaret","New Wave",
    "Psychadelic","Rave","ShowTunes","Trailer","Lo-Fi","Tribal","Acid Punk","Acid Jazz",
    "Polka","Retro","Musical","Rock & Roll","Hard Rock","Folk","Folk Rock",
    "National Folk","Swing","Fast Fusion","Bebob","Latin","Revival","Celtic",
    "Bluegrass","Avantgarde","Gothic Rock","Progressive Rock","Psychedelic Rock",
    "Symphonic Rock","Slow Rock","Big Band","Chorus","Easy Listening","Acoustic",
    "Humour","Speech","Chanson","Opera","Chamber Music","Sonata","Symphony","Booty Bass",
    "Primus","Porn Groove","Satire","Slow Jam","Club","Tango","Samba","Folklore","Ballad",
    "Power Ballad","Rhytmic Soul","Freestyle","Duet","Punk Rock","Drum Solo","A Capella",
    "Euro House","Dance Hall","Goa","Drum & Bass","Club House","Hardcore","Terror",
    "Indie","BritPop","Negerpunk","Polsk Punk","Beat","Christian Gangsta Rap",
    "Heavy Metal","Black Metal","Crossover","Contemporary Christian","Christian Rock",
    "Merengue","Salsa","Trash Metal","Anime","JPop","SynthPop"
};
//定义一些使用到的结构体
//define frame id (see in ID3v2.3)
//#define ID3V2_FRAMEID_TITLE     "TIT2"    //Title
//#define ID3V2_FRAMEID_ARTIST    "TPE1"    //Artist
//#define ID3V2_FRAMEID_ALBUM     "TALB"    //Album
//#define ID3V2_FRAMEID_TRACK     "TRCK"    //Track
//#define ID3V2_FRAMEID_YEAR      "TYER"    //Year
//#define ID3V2_FRAMEID_COMMENT   "COMM"    //Comment
//#define ID3V2_FRAMEID_GENRE     "TCON"    //Genre
//#define ID3V2_FRAMEID_ENCODEBY  "TENC"    //Encode By(编码方式)
//#define ID3V2_FRAMEID_COPYRIGHT "TCOP"    //Copyright
//#define  ID3V2_FRAMEID_URL       "WXXX"    //URL
 
STATIC LPCTSTR m_pszID3V2FrameId[] = 
{
  "????", /* ID3FID_NOFRAME = 0,        */ /**< No known frame */
  "AENC", /* ID3FID_AUDIOCRYPTO,        */ /**< Audio encryption */
  "APIC", /* ID3FID_PICTURE,            */ /**< Attached picture */
  "ASPI", /* ID3FID_AUDIOSEEKPOINT,     */ /**< Audio seek point index */
  "COMM", /* ID3FID_COMMENT,            */ /**< Comments */
  "COMR", /* ID3FID_COMMERCIAL,         */ /**< Commercial frame */
  "ENCR", /* ID3FID_CRYPTOREG,          */ /**< Encryption method registration */
  "EQU2", /* ID3FID_EQUALIZATION2,      */ /**< Equalisation (2) */
  "EQUA", /* ID3FID_EQUALIZATION,       */ /**< Equalization */
  "ETCO", /* ID3FID_EVENTTIMING,        */ /**< Event timing codes */
  "GEOB", /* ID3FID_GENERALOBJECT,      */ /**< General encapsulated object */
  "GRID", /* ID3FID_GROUPINGREG,        */ /**< Group identification registration */
  "IPLS", /* ID3FID_INVOLVEDPEOPLE,     */ /**< Involved people list */
  "LINK", /* ID3FID_LINKEDINFO,         */ /**< Linked information */
  "MCDI", /* ID3FID_CDID,               */ /**< Music CD identifier */
  "MLLT", /* ID3FID_MPEGLOOKUP,         */ /**< MPEG location lookup table */
  "OWNE", /* ID3FID_OWNERSHIP,          */ /**< Ownership frame */
  "PRIV", /* ID3FID_PRIVATE,            */ /**< Private frame */
  "PCNT", /* ID3FID_PLAYCOUNTER,        */ /**< Play counter */
  "POPM", /* ID3FID_POPULARIMETER,      */ /**< Popularimeter */
  "POSS", /* ID3FID_POSITIONSYNC,       */ /**< Position synchronisation frame */
  "RBUF", /* ID3FID_BUFFERSIZE,         */ /**< Recommended buffer size */
  "RVA2", /* ID3FID_VOLUMEADJ2,         */ /**< Relative volume adjustment (2) */
  "RVAD", /* ID3FID_VOLUMEADJ,          */ /**< Relative volume adjustment */
  "RVRB", /* ID3FID_REVERB,             */ /**< Reverb */
  "SEEK", /* ID3FID_SEEKFRAME,          */ /**< Seek frame */
  "SIGN", /* ID3FID_SIGNATURE,          */ /**< Signature frame */
  "SYLT", /* ID3FID_SYNCEDLYRICS,       */ /**< Synchronized lyric/text */
  "SYTC", /* ID3FID_SYNCEDTEMPO,        */ /**< Synchronized tempo codes */
  "TALB", /* ID3FID_ALBUM,              */ /**< Album/Movie/Show title */
  "TBPM", /* ID3FID_BPM,                */ /**< BPM (beats per minute) */
  "TCOM", /* ID3FID_COMPOSER,           */ /**< Composer */
  "TCON", /* ID3FID_CONTENTTYPE,        */ /**< Content type */
  "TCOP", /* ID3FID_COPYRIGHT,          */ /**< Copyright message */
  "TDAT", /* ID3FID_DATE,               */ /**< Date */
  "TDEN", /* ID3FID_ENCODINGTIME,       */ /**< Encoding time */
  "TDLY", /* ID3FID_PLAYLISTDELAY,      */ /**< Playlist delay */
  "TDOR", /* ID3FID_ORIGRELEASETIME,    */ /**< Original release time */
  "TDRC", /* ID3FID_RECORDINGTIME,      */ /**< Recording time */
  "TDRL", /* ID3FID_RELEASETIME,        */ /**< Release time */
  "TDTG", /* ID3FID_TAGGINGTIME,        */ /**< Tagging time */
  "TIPL", /* ID3FID_INVOLVEDPEOPLE2,    */ /**< Involved people list */
  "TENC", /* ID3FID_ENCODEDBY,          */ /**< Encoded by */
  "TEXT", /* ID3FID_LYRICIST,           */ /**< Lyricist/Text writer */
  "TFLT", /* ID3FID_FILETYPE,           */ /**< File type */
  "TIME", /* ID3FID_TIME,               */ /**< Time */
  "TIT1", /* ID3FID_CONTENTGROUP,       */ /**< Content group description */
  "TIT2", /* ID3FID_TITLE,              */ /**< Title/songname/content description */
  "TIT3", /* ID3FID_SUBTITLE,           */ /**< Subtitle/Description refinement */
  "TKEY", /* ID3FID_INITIALKEY,         */ /**< Initial key */
  "TLAN", /* ID3FID_LANGUAGE,           */ /**< Language(s) */
  "TLEN", /* ID3FID_SONGLEN,            */ /**< Length */
  "TMCL", /* ID3FID_MUSICIANCREDITLIST, */ /**< Musician credits list */
  "TMED", /* ID3FID_MEDIATYPE,          */ /**< Media type */
  "TMOO", /* ID3FID_MOOD,               */ /**< Mood */
  "TOAL", /* ID3FID_ORIGALBUM,          */ /**< Original album/movie/show title */
  "TOFN", /* ID3FID_ORIGFILENAME,       */ /**< Original filename */
  "TOLY", /* ID3FID_ORIGLYRICIST,       */ /**< Original lyricist(s)/text writer(s) */
  "TOPE", /* ID3FID_ORIGARTIST,         */ /**< Original artist(s)/performer(s) */
  "TORY", /* ID3FID_ORIGYEAR,           */ /**< Original release year */
  "TOWN", /* ID3FID_FILEOWNER,          */ /**< File owner/licensee */
  "TPE1", /* ID3FID_LEADARTIST,         */ /**< Lead performer(s)/Soloist(s) */
  "TPE2", /* ID3FID_BAND,               */ /**< Band/orchestra/accompaniment */
  "TPE3", /* ID3FID_CONDUCTOR,          */ /**< Conductor/performer refinement */
  "TPE4", /* ID3FID_MIXARTIST,          */ /**< Interpreted, remixed, or otherwise modified by */
  "TPOS", /* ID3FID_PARTINSET,          */ /**< Part of a set */
  "TPRO", /* ID3FID_PRODUCEDNOTICE,     */ /**< Produced notice */
  "TPUB", /* ID3FID_PUBLISHER,          */ /**< Publisher */
  "TRCK", /* ID3FID_TRACKNUM,           */ /**< Track number/Position in set */
  "TRDA", /* ID3FID_RECORDINGDATES,     */ /**< Recording dates */
  "TRSN", /* ID3FID_NETRADIOSTATION,    */ /**< Internet radio station name */
  "TRSO", /* ID3FID_NETRADIOOWNER,      */ /**< Internet radio station owner */
  "TSIZ", /* ID3FID_SIZE,               */ /**< Size */
  "TSOA", /* ID3FID_ALBUMSORTORDER,     */ /**< Album sort order */
  "TSOP", /* ID3FID_PERFORMERSORTORDER, */ /**< Performer sort order */
  "TSOT", /* ID3FID_TITLESORTORDER,     */ /**< Title sort order */
  "TSRC", /* ID3FID_ISRC,               */ /**< ISRC (international standard recording code) */
  "TSSE", /* ID3FID_ENCODERSETTINGS,    */ /**< Software/Hardware and settings used for encoding */
  "TSST", /* ID3FID_SETSUBTITLE,        */ /**< Set subtitle */
  "TXXX", /* ID3FID_USERTEXT,           */ /**< User defined text information */
  "TYER", /* ID3FID_YEAR,               */ /**< Year */
  "UFID", /* ID3FID_UNIQUEFILEID,       */ /**< Unique file identifier */
  "USER", /* ID3FID_TERMSOFUSE,         */ /**< Terms of use */
  "USLT", /* ID3FID_UNSYNCEDLYRICS,     */ /**< Unsynchronized lyric/text transcription */
  "WCOM", /* ID3FID_WWWCOMMERCIALINFO,  */ /**< Commercial information */
  "WCOP", /* ID3FID_WWWCOPYRIGHT,       */ /**< Copyright/Legal infromation */
  "WOAF", /* ID3FID_WWWAUDIOFILE,       */ /**< Official audio file webpage */
  "WOAR", /* ID3FID_WWWARTIST,          */ /**< Official artist/performer webpage */
  "WOAS", /* ID3FID_WWWAUDIOSOURCE,     */ /**< Official audio source webpage */
  "WORS", /* ID3FID_WWWRADIOPAGE,       */ /**< Official internet radio station homepage */
  "WPAY", /* ID3FID_WWWPAYMENT,         */ /**< Payment */
  "WPUB", /* ID3FID_WWWPUBLISHER,       */ /**< Official publisher webpage */
  "WXXX", /* ID3FID_WWWUSER,            */ /**< User defined URL link */
  "    ", /* ID3FID_METACRYPTO,         */ /**< Encrypted meta frame (id3v2.2.x) */
  "    ", /* ID3FID_METACOMPRESSION,    */ /**< Compressed meta frame (id3v2.2.1) */
  ">>>>"  /* ID3FID_LASTFRAMEID         */ /**< Last field placeholder */
};
//
typedef struct _tagID3v2FrameHeader
{
    TCHAR   ifh_id[4];          //标志位
    UCHAR   ifh_pszlength[4];   //接下来那个 TAG 的长度
    UCHAR   ifh_flags[2];       //标志。在当前版本中不使用(2005-10-24)
  TCHAR   ifh_pad;            //填充。必须为 
  //整个 Frame 的大小为 ifh_pszlength(需要进行计算转化)+=11
  DWORD   ifh_info_length;    //ifh_pszlength转化过来的长度
  DWORD   ifh_size;           //Frame 的大小
  DWORD   ifh_pos;            //这个 Frame 在文件中的位置(相对于文件起始)
}ID3v2FrameHeader;
 
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//                  下面开始读取 MP3ID3V1 信息
//
//////////////////////////////////////////////////////////////////////////
 
/*********************************************************************
  函数声明: int AudioInfo_GetMP3ID3V1(HFILE hAudioFile, MP3ID3V1INFO *pMp3ID3V1Info)
  参    数:
          IN: HFILE hMP3File: 需要读取信息的 MP3 文件
              MP3ID3V1INFO *pMp3ID3V1Info: 读取了 MP3  信息的返回结构体缓冲
         OUT: 
         I/O: 
    返回值: 成功返回>=0，失败返回<0。这时可以使用函数 Audio_GetErrorString 来获取失败信息 
            如果该 MP3 文件中存在 ID3V1，那么 bHasTag == TRUE
  功能描述: 读取 MP3 中的 ID3V1 信息
  引    用: 
*********************************************************************/
int AudioInfo_GetMP3ID3V1(HFILE hMP3File, MP3ID3V1INFO *pMP3ID3V1Info)
{
  DWORD dwFileRead = 0;
  int i = 0;
  memset(pMP3ID3V1Info, 0x00, sizeof(MP3ID3V1INFO));
  //判断该文件的长度是否的>128字节
  SetFilePointer(hMP3File, 0x00, 0, FILE_BEGIN);
  dwFileRead = SetFilePointer(hMP3File, 0x00, 0, FILE_END);
  if(dwFileRead<=128)
  {
    AUDIO_ERR_OUT("这个文件不是一个有效的 MP3 文件");
    return -1;
  }
  SetFilePointer(hMP3File, -128, 0, FILE_END);
  //一次性读取这128个字节
  ReadFile(hMP3File, pMP3ID3V1Info, 128, &dwFileRead, NULL);
  if(dwFileRead != 128)
  {
    AUDIO_ERR_OUT("这个文件不是一个有效的 MP3 文件");
    return -1;
  }
  pMP3ID3V1Info->Genre = pMP3ID3V1Info->CommentFlag;
  pMP3ID3V1Info->CommentFlag = pMP3ID3V1Info->Comment[28];
  pMP3ID3V1Info->Track = pMP3ID3V1Info->Comment[29];
  //判断是否拥有 ID3V1 信息
  if(pMP3ID3V1Info->Identify[0]!='T'
    || pMP3ID3V1Info->Identify[1]!='A'
    || pMP3ID3V1Info->Identify[2]!='G')
  {
    AUDIO_ERR_OUT("这个 MP3 中不包含 ID3V1 信息");
    pMP3ID3V1Info->bHasTag = FALSE;
    return -1;
  }
  //判断 TAG 信息是否有效。就是判断剩余的 125 个字节是否全部是空格或是 
  for(i=3;i<128;i++)
  {
    if(((UCHAR*)pMP3ID3V1Info)[i] != 0x20 || ((UCHAR*)pMP3ID3V1Info)[i] != 0x00)
    {
      pMP3ID3V1Info->bHasTag = TRUE;
      break;
    }
  }
  if(!pMP3ID3V1Info->bHasTag)
  {
    AUDIO_ERR_OUT("这个 MP3 中 ID3V1 信息不是有效的");
    return -1;
  }
  //接下来分析。
  //首先分析注释的长度
  //if(pMP3ID3V1Info->CommentFlag != 0x00)    //如果注释长度不是28个字节
  //{
  //}
  //有些软件会把 Title, Artist 等信息最后为 0x20 而不是
  //下面取出末尾的 0x20
  //去除 Title
  pMP3ID3V1Info->Title[29] = '';
  for(i=28;
      i>=0&&(pMP3ID3V1Info->Title[i]==0x20 || pMP3ID3V1Info->Title[i]==0x00);
      i--)
  {
    pMP3ID3V1Info->Title[i] = '';
  }
  //去除 Artist
  pMP3ID3V1Info->Artist[29] = '';
  for(i=28;
      i>=0&&(pMP3ID3V1Info->Artist[i]==0x20 || pMP3ID3V1Info->Artist[i]==0x00);
      i--)
  {
    pMP3ID3V1Info->Artist[i] = '';
  }
  //去除 Album
  pMP3ID3V1Info->Album[29] = '';
  for(i=28;
      i>=0&&(pMP3ID3V1Info->Album[i]==0x20 || pMP3ID3V1Info->Album[i]==0x00);
      i--)
  {
    pMP3ID3V1Info->Album[i] = '';
  }
  //保存 音乐的类型
  ASSERT(pMP3ID3V1Info->Genre>=0 && pMP3ID3V1Info->Genre<m_nMP3ID3v2GenreCount);
  strcpy(pMP3ID3V1Info->pszGenre, m_arrMP3ID3v2Genre[pMP3ID3V1Info->Genre]);
  return 0;
}
//////////////////////////////////////////////////////////////////////////
//
//                  下面开始读取 MP3 ID3V2 信息
//
//////////////////////////////////////////////////////////////////////////
 
/*********************************************************************
  函数声明: 
  参    数:
          IN: 
         OUT: 
         I/O: 
    返回值: 
  功能描述: 判断这个 MP3 中是否包含 ID3V2 信息。如果存在返回TRUE，否则不存在
            并且获取这个ID3V2 信息的长度。
  引    用: 
*********************************************************************/
STATIC BOOL AudioInfo_CheckMP3ID3V2(HFILE hMP3File, MP3ID3V2INFO *pMP3ID3V2Info)
{
  DWORD dwFileRead = 0;
  int i = 0;
  DWORD dwID3V2Length = 0;
  pMP3ID3V2Info->bHasTag = FALSE;
  //读取 MP3 文件的最前面 3 个字节，判断是否是 "ID3"
  ReadFile(hMP3File, (VOID*)pMP3ID3V2Info, 10, &dwFileRead, NULL);
  if(dwFileRead != 10)
  {
    return FALSE;
  }
  if( pMP3ID3V2Info->Identify[0]=='I'
    &&pMP3ID3V2Info->Identify[1]=='D'
    &&pMP3ID3V2Info->Identify[2]=='3'
    )
  {
    pMP3ID3V2Info->bHasTag = TRUE;
  }
  else
  {
    return FALSE;
  }
  //判断是否含有扩展头部和扩展尾部
    if(pMP3ID3V2Info->HeaderFlags&0x64==0x64)
  {
    pMP3ID3V2Info->bHasExtHeader = TRUE;
  }
    if(pMP3ID3V2Info->HeaderFlags&0x10==0x10)
  {
    pMP3ID3V2Info->bHasExtHeader = TRUE;
  }
  //获取 ID3V1 信息的长度
    for(i=0;i<3;i++)
  {
    dwID3V2Length = dwID3V2Length + pMP3ID3V2Info->HeaderSize[i];
    dwID3V2Length = dwID3V2Length * 0x80;
  }
  dwID3V2Length = dwID3V2Length + pMP3ID3V2Info->HeaderSize[3];
  pMP3ID3V2Info->dwInfoSize = dwID3V2Length;
  return TRUE;
}
 
/*********************************************************************
  函数声明: 
  参    数:
          IN: 
         OUT: 
         I/O: 
    返回值: 返回 Frame 的个数
  功能描述: 获取 ID3V2 中的 FrameHeader
  引    用: 
*********************************************************************/
STATIC UINT AudioInfo_GetFrameHeader(HFILE hMP3File,  MP3ID3V2INFO *pMP3ID3V2Info)
{
  DWORD dwFileRead = 0;
  DWORD dwFilePos = 0;
  UINT  uFrameCount = 0;
  int i = 0;
  ID3v2FrameHeader tFrmHeader = {0};
  UINT uFrameID = ID3FID_NOFRAME;
  for(dwFilePos=10; dwFilePos<pMP3ID3V2Info->dwInfoSize;)
  {
    SetFilePointer(hMP3File, dwFilePos, 0, FILE_BEGIN);
    ReadFile(hMP3File, (VOID*)&tFrmHeader, 11, &dwFileRead, NULL);
    ASSERT(dwFileRead == 11);
    dwFilePos += dwFileRead;
    if(tFrmHeader.ifh_id[0] != 0x00)
    {
      uFrameCount++;      //Frame 个数 +1
      tFrmHeader.ifh_info_length = 0;
      for(i=0;i<3;i++)
      {
        tFrmHeader.ifh_info_length = tFrmHeader.ifh_info_length+tFrmHeader.ifh_pszlength[i];
        tFrmHeader.ifh_info_length = tFrmHeader.ifh_info_length*0x80;
      }
      tFrmHeader.ifh_info_length = tFrmHeader.ifh_info_length + tFrmHeader.ifh_pszlength[3];
      if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_NOFRAME], 4)==0)
      {
        uFrameID = ID3FID_NOFRAME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_AUDIOCRYPTO], 4)==0)
      {
        uFrameID = ID3FID_AUDIOCRYPTO;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PICTURE], 4)==0)
      {
        uFrameID = ID3FID_PICTURE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_AUDIOSEEKPOINT], 4)==0)
      {
        uFrameID = ID3FID_AUDIOSEEKPOINT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_COMMENT], 4)==0)
      {
        uFrameID = ID3FID_COMMENT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_COMMERCIAL], 4)==0)
      {
        uFrameID = ID3FID_COMMERCIAL;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_CRYPTOREG], 4)==0)
      {
        uFrameID = ID3FID_CRYPTOREG;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_EQUALIZATION2], 4)==0)
      {
        uFrameID = ID3FID_EQUALIZATION2;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_EQUALIZATION], 4)==0)
      {
        uFrameID = ID3FID_EQUALIZATION;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_EVENTTIMING], 4)==0)
      {
        uFrameID = ID3FID_EVENTTIMING;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_GENERALOBJECT], 4)==0)
      {
        uFrameID = ID3FID_GENERALOBJECT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_GROUPINGREG], 4)==0)
      {
        uFrameID = ID3FID_GROUPINGREG;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_INVOLVEDPEOPLE], 4)==0)
      {
        uFrameID = ID3FID_INVOLVEDPEOPLE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_LINKEDINFO], 4)==0)
      {
        uFrameID = ID3FID_LINKEDINFO;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_CDID], 4)==0)
      {
        uFrameID = ID3FID_CDID;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_MPEGLOOKUP], 4)==0)
      {
        uFrameID = ID3FID_MPEGLOOKUP;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_OWNERSHIP], 4)==0)
      {
        uFrameID = ID3FID_OWNERSHIP;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PRIVATE], 4)==0)
      {
        uFrameID = ID3FID_PRIVATE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PLAYCOUNTER], 4)==0)
      {
        uFrameID = ID3FID_PLAYCOUNTER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_POPULARIMETER], 4)==0)
      {
        uFrameID = ID3FID_POPULARIMETER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_POSITIONSYNC], 4)==0)
      {
        uFrameID = ID3FID_POSITIONSYNC;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_BUFFERSIZE], 4)==0)
      {
        uFrameID = ID3FID_BUFFERSIZE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_VOLUMEADJ2], 4)==0)
      {
        uFrameID = ID3FID_VOLUMEADJ2;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_VOLUMEADJ], 4)==0)
      {
        uFrameID = ID3FID_VOLUMEADJ;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_REVERB], 4)==0)
      {
        uFrameID = ID3FID_REVERB;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SEEKFRAME], 4)==0)
      {
        uFrameID = ID3FID_SEEKFRAME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SIGNATURE], 4)==0)
      {
        uFrameID = ID3FID_SIGNATURE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SYNCEDLYRICS], 4)==0)
      {
        uFrameID = ID3FID_SYNCEDLYRICS;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SYNCEDTEMPO], 4)==0)
      {
        uFrameID = ID3FID_SYNCEDTEMPO;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ALBUM], 4)==0)
      {
        uFrameID = ID3FID_ALBUM;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_BPM], 4)==0)
      {
        uFrameID = ID3FID_BPM;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_COMPOSER], 4)==0)
      {
        uFrameID = ID3FID_COMPOSER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_CONTENTTYPE], 4)==0)
      {
        uFrameID = ID3FID_CONTENTTYPE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_COPYRIGHT], 4)==0)
      {
        uFrameID = ID3FID_COPYRIGHT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_DATE], 4)==0)
      {
        uFrameID = ID3FID_DATE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ENCODINGTIME], 4)==0)
      {
        uFrameID = ID3FID_ENCODINGTIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PLAYLISTDELAY], 4)==0)
      {
        uFrameID = ID3FID_PLAYLISTDELAY;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGRELEASETIME], 4)==0)
      {
        uFrameID = ID3FID_ORIGRELEASETIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_RECORDINGTIME], 4)==0)
      {
        uFrameID = ID3FID_RECORDINGTIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_RELEASETIME], 4)==0)
      {
        uFrameID = ID3FID_RELEASETIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TAGGINGTIME], 4)==0)
      {
        uFrameID = ID3FID_TAGGINGTIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_INVOLVEDPEOPLE2], 4)==0)
      {
        uFrameID = ID3FID_INVOLVEDPEOPLE2;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ENCODEDBY], 4)==0)
      {
        uFrameID = ID3FID_ENCODEDBY;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_LYRICIST], 4)==0)
      {
        uFrameID = ID3FID_LYRICIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_FILETYPE], 4)==0)
      {
        uFrameID = ID3FID_FILETYPE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TIME], 4)==0)
      {
        uFrameID = ID3FID_TIME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_CONTENTGROUP], 4)==0)
      {
        uFrameID = ID3FID_CONTENTGROUP;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TITLE], 4)==0)
      {
        uFrameID = ID3FID_TITLE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SUBTITLE], 4)==0)
      {
        uFrameID = ID3FID_SUBTITLE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_INITIALKEY], 4)==0)
      {
        uFrameID = ID3FID_INITIALKEY;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_LANGUAGE], 4)==0)
      {
        uFrameID = ID3FID_LANGUAGE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SONGLEN], 4)==0)
      {
        uFrameID = ID3FID_SONGLEN;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_MUSICIANCREDITLIST], 4)==0)
      {
        uFrameID = ID3FID_MUSICIANCREDITLIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_MEDIATYPE], 4)==0)
      {
        uFrameID = ID3FID_MEDIATYPE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_MOOD], 4)==0)
      {
        uFrameID = ID3FID_MOOD;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGALBUM], 4)==0)
      {
        uFrameID = ID3FID_ORIGALBUM;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGFILENAME], 4)==0)
      {
        uFrameID = ID3FID_ORIGFILENAME;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGLYRICIST], 4)==0)
      {
        uFrameID = ID3FID_ORIGLYRICIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGARTIST], 4)==0)
      {
        uFrameID = ID3FID_ORIGARTIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ORIGYEAR], 4)==0)
      {
        uFrameID = ID3FID_ORIGYEAR;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_FILEOWNER], 4)==0)
      {
        uFrameID = ID3FID_FILEOWNER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_LEADARTIST], 4)==0)
      {
        uFrameID = ID3FID_LEADARTIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_BAND], 4)==0)
      {
        uFrameID = ID3FID_BAND;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_CONDUCTOR], 4)==0)
      {
        uFrameID = ID3FID_CONDUCTOR;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_MIXARTIST], 4)==0)
      {
        uFrameID = ID3FID_MIXARTIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PARTINSET], 4)==0)
      {
        uFrameID = ID3FID_PARTINSET;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PRODUCEDNOTICE], 4)==0)
      {
        uFrameID = ID3FID_PRODUCEDNOTICE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PUBLISHER], 4)==0)
      {
        uFrameID = ID3FID_PUBLISHER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TRACKNUM], 4)==0)
      {
        uFrameID = ID3FID_TRACKNUM;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_RECORDINGDATES], 4)==0)
      {
        uFrameID = ID3FID_RECORDINGDATES;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_NETRADIOSTATION], 4)==0)
      {
        uFrameID = ID3FID_NETRADIOSTATION;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_NETRADIOOWNER], 4)==0)
      {
        uFrameID = ID3FID_NETRADIOOWNER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SIZE], 4)==0)
      {
        uFrameID = ID3FID_SIZE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ALBUMSORTORDER], 4)==0)
      {
        uFrameID = ID3FID_ALBUMSORTORDER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_PERFORMERSORTORDER], 4)==0)
      {
        uFrameID = ID3FID_PERFORMERSORTORDER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TITLESORTORDER], 4)==0)
      {
        uFrameID = ID3FID_TITLESORTORDER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ISRC], 4)==0)
      {
        uFrameID = ID3FID_ISRC;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_ENCODERSETTINGS], 4)==0)
      {
        uFrameID = ID3FID_ENCODERSETTINGS;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_SETSUBTITLE], 4)==0)
      {
        uFrameID = ID3FID_SETSUBTITLE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_USERTEXT], 4)==0)
      {
        uFrameID = ID3FID_USERTEXT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_YEAR], 4)==0)
      {
        uFrameID = ID3FID_YEAR;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_UNIQUEFILEID], 4)==0)
      {
        uFrameID = ID3FID_UNIQUEFILEID;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_TERMSOFUSE], 4)==0)
      {
        uFrameID = ID3FID_TERMSOFUSE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_UNSYNCEDLYRICS], 4)==0)
      {
        uFrameID = ID3FID_UNSYNCEDLYRICS;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWCOMMERCIALINFO], 4)==0)
      {
        uFrameID = ID3FID_WWWCOMMERCIALINFO;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWCOPYRIGHT], 4)==0)
      {
        uFrameID = ID3FID_WWWCOPYRIGHT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWAUDIOFILE], 4)==0)
      {
        uFrameID = ID3FID_WWWAUDIOFILE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWARTIST], 4)==0)
      {
        uFrameID = ID3FID_WWWARTIST;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWAUDIOSOURCE], 4)==0)
      {
        uFrameID = ID3FID_WWWAUDIOSOURCE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWRADIOPAGE], 4)==0)
      {
        uFrameID = ID3FID_WWWRADIOPAGE;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWPAYMENT], 4)==0)
      {
        uFrameID = ID3FID_WWWPAYMENT;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWPUBLISHER], 4)==0)
      {
        uFrameID = ID3FID_WWWPUBLISHER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_WWWUSER], 4)==0)
      {
        uFrameID = ID3FID_WWWUSER;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_METACRYPTO], 4)==0)
      {
        uFrameID = ID3FID_METACRYPTO;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_METACOMPRESSION], 4)==0)
      {
        uFrameID = ID3FID_METACOMPRESSION;
      }
      else if(strncmp(tFrmHeader.ifh_id, m_pszID3V2FrameId[ID3FID_LASTFRAMEID], 4)==0)
      {
        uFrameID = ID3FID_LASTFRAMEID;
      }
      else
      {
        ASSERT(FALSE);
        uFrameID = ID3FID_LASTFRAMEID;
      }
      //上面已经计算出了偏移量及长度和求出了 FrameID，下面从文件中读取这个信息
      pMP3ID3V2Info->ppszTagInfo[uFrameID] = malloc(tFrmHeader.ifh_info_length);
      memset(pMP3ID3V2Info->ppszTagInfo[uFrameID], 0x00, tFrmHeader.ifh_info_length);
      ReadFile(hMP3File, pMP3ID3V2Info->ppszTagInfo[uFrameID], tFrmHeader.ifh_info_length-1, &dwFileRead, NULL);
      dwFilePos += dwFileRead;
    }
  }
  return uFrameCount;
}
 
/*********************************************************************
  函数声明: VOID AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info)
  参    数:
          IN: MP3ID3V2INFO *pMP3ID3V2Info: 需要清空的结构体
         OUT: 
         I/O: 
    返回值: 
  功能描述: 清空 MP3ID3V2INFO 中的信息，回收内存
  引    用: 
*********************************************************************/
VOID AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info)
{
  UINT uFrameID = 0;
  ASSERT(pMP3ID3V2Info!=NULL);
  for(uFrameID=0; uFrameID<ID3FID_MAX_COUNT;uFrameID++)
  {
    if(pMP3ID3V2Info->ppszTagInfo[uFrameID]!=NULL)
    {
      free(pMP3ID3V2Info->ppszTagInfo[uFrameID]);
      pMP3ID3V2Info->ppszTagInfo[uFrameID] = NULL;
    }
  }
}
/*********************************************************************
  函数声明: int AudioInfo_GetMP3ID3V2(HFILE hMP3File, MP3ID3V2INFO *pMP3ID3V2Info)
  参    数:
          IN: HFILE hMP3File: 需要读取信息的 MP3 文件
              MP3ID3V2INFO *pMp3ID3V2Info: 读取了 MP3  信息的返回结构体缓冲
         OUT: 
         I/O: 
    返回值: 成功返回>=0，失败返回<0。这时可以使用函数 Audio_GetErrorString 来获取失败信息
            如果该 MP3 文件中存在 ID3V2，那么 bHasTag == TRUE
  功能描述: 读取 MP3 中的 ID3V2 信息
      注意: 使用ID3V2信息完毕后，请调用函数 AudioInfo_MP3ID3V2Free(MP3ID3V2INFO *pMP3ID3V2Info)
  引    用: 
*********************************************************************/
int AudioInfo_GetMP3ID3V2(HFILE hMP3File, MP3ID3V2INFO *pMP3ID3V2Info)
{
  DWORD dwFileRead = 0;
  ASSERT(pMP3ID3V2Info != NULL);
  memset(pMP3ID3V2Info, 0x00, sizeof(MP3ID3V2INFO));
  SetFilePointer(hMP3File, 0x00, 0, FILE_BEGIN);
  //检查文件中中是否有 ID3V2 Tag 信息
  if(!AudioInfo_CheckMP3ID3V2(hMP3File, pMP3ID3V2Info))
  {
    ASSERT(pMP3ID3V2Info->bHasTag == FALSE);
    return -1;
  }
  //检查 MP3 文件是否含有 ID3V2 信息完毕。到这一步确定已经包含 ID3V2 信息
  //并且已经得到了 ID3V2 信息的长度
 
  //在本版本中，仅仅支持 ID3V2 信息的 V3 版本或以下
  //这个版本中，不支持 ExtHeader，所以下面判断是否存在 ExtHeader
  if(pMP3ID3V2Info->bHasExtHeader)
  {
    return -1;
  }
  //获取 ID3V2 FrameHeader
  AudioInfo_GetFrameHeader(hMP3File, pMP3ID3V2Info);
  return 0;
}
