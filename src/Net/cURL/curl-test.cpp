
//　首先需要加入多线程的机制，因为程序一边在下载文件，一边在显示进度条，单线程的方式肯定不行，所以我用到了wxTimer来实现,在downloadMain.h 中定义了一个wxTimer，并做了事件申明.
DECLARE_EVENT_TABLE()

#ifndef DOWNLOADMAIN_H
#define DOWNLOADMAIN_H

#include "downloadApp.h"
#include 
#include "GUIDialog.h"


class downloadDialog: public GUIDialog
{
public:
    downloadDialog(wxDialog *dlg);
    ~downloadDialog();
    void OnTimer(wxTimerEvent& event);
private:
    virtual void OnClose(wxCloseEvent& event);
    virtual void OnQuit(wxCommandEvent& event);
    virtual void OnAbout(wxCommandEvent& event);
    void downloadfile();
    wxTimer* m_timerdown;
    DECLARE_EVENT_TABLE()
};
#endif // DOWNLOADMAIN_H


//下面是主程序的代码.
#ifdef WX_PRECOMP
#include "wx_pch.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#include "downloadMain.h"
#include 
#include 
#include 
#include "update.h"
#include 
#include 
#define TIMER_ID 22222



//事件监听声明
BEGIN_EVENT_TABLE(downloadDialog, GUIDialog)
EVT_TIMER(TIMER_ID, downloadDialog::OnTimer)
END_EVENT_TABLE()
enum wxbuildinfoformat {
    short_f, 
    long_f
};


wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);
    
    if (format == long_f ) {
        #if defined(__WXMSW__)
        wxbuild << _T("-Windows");
        #elif defined(__WXMAC__)
        wxbuild << _T("-Mac");
        #elif defined(__UNIX__)
        wxbuild << _T("-Linux");
        #endif
        #if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
        #else
        wxbuild << _T("-ANSI build");
        #endif // wxUSE_UNICODE
    }
    
    return wxbuild;
}


//声明一个文件结构体
struct FtpFile {
    char *filename;
    FILE *stream;
};

downloadDialog::downloadDialog(wxDialog *dlg)
    : GUIDialog(dlg)
{
    //创建一个定时器,制定TIMER_ID
    m_timerdown = new wxTimer(this, TIMER_ID);
    //定时器开始运行,这里会自动执行OnTimer函数
    m_timerdown->Start(100);
}

downloadDialog::~downloadDialog()
{
}



//定时器操作
void downloadDialog::OnTimer(wxTimerEvent &event)
{
    downloadfile();
}
//文件写入流
int my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
    struct FtpFile *out=(struct FtpFile *)stream;
    if (out && !out->stream) {
        out->stream=fopen(out->filename, "wb");
        if (!out->stream) {
            return -1;
        }
    }
    return fwrite(buffer, size, nmemb, out->stream);
}


//进度条显示函数
int wxcurldav_dl_progress_func(void* ptr, double rDlTotal, double rDlNow, double rUlTotal, double rUlNow)
{
    wxGauge* pGauge = (wxGauge*) ptr;
    
    if(pGauge)
        pGauge->SetValue(100.0 * (rDlNow/rDlTotal)); //设置进度条的值
    return 0;
}


//下载文件函数
void downloadDialog::downloadfile()
{
    //创建curl对象
    CURL *curl;
    CURLcode res;
    m_staticText2->SetLabel(wxT("请耐心等待程序下载更新包..."));
    
    struct FtpFile ftpfile= {
        //定义下载到本地的文件位置和路径
        "tmp.exe",
        NULL
    };
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    //curl初始化
    curl = curl_easy_init();
    
    //curl对象存在的情况下执行操作
    if (curl) {
        //设置远端地址
        curl_easy_setopt(curl, CURLOPT_URL,"http://dl_dir.qq.com/minigamefile/QQGame2008ReleaseP2_web_setup.EXE");
        
        //执行写入文件流操作
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpfile);
        
        //curl的进度条声明
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
        
        //回调进度条函数
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, wxcurldav_dl_progress_func);
        
        //设置进度条名称
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, m_gauge1);
        
        //进度条
        m_gauge1->SetValue(0);
        
        //写入文件
        res = curl_easy_perform(curl);
        m_gauge1->SetValue(100);
        
        //释放curl对象
        curl_easy_cleanup(curl);
        if (CURLE_OK != res)
            ;
    }
    
    if (ftpfile.stream) {
        //关闭文件流
        fclose(ftpfile.stream);
    }
    
    //释放全局curl对象
    curl_global_cleanup();
    
    //这一步很重要，停止定时器,不然程序会无休止的运行下去
    m_timerdown->Stop();
    
    //执行刚下载完毕的程序,进行程序更新
    int pid = ::wxExecute(_T("tmp.exe"));
    wxMessageBox(wxT("下载完毕，程序开始执行更新操作......"));
    
    return ;
}


void downloadDialog::OnClose(wxCloseEvent &event)
{
    Destroy();
}


void downloadDialog::OnQuit(wxCommandEvent &event)
{
    Destroy();
}


void downloadDialog::OnAbout(wxCommandEvent &event)
{
}










