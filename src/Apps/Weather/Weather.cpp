#include <iostream>
#include <sstream>
#include <string>
#include <cassert>
#include <iomanip>

extern "C" {
    #include <stdio.h>
    #include "HttpFetcher.h"
}
#include "includes/json.h"
#include "Weather.h"

#define     WEATHER_LOG             printf
#define     WEATHER_LOG_WARNING     printf


using namespace std;
using namespace Json;

/**
* value: 0---7
*   1st bit: city id status;
*    	0 - http get city id OK;
*    	1 - http get city id is Error;
*
*   2nd bit: weather info get status;
*    	0 - http get weather info OK;
*    	1 - http get weather info is Error;
*
*   3th bit: aqi info get status;
*    	0 - http get aqi info OK;
*      	1 - http get aqi info is Error;
*
**/
static int isErrorStatus = 0;
static std::string g_cityID;
static const int LengthOfCityID = 9; /**  9 : length of city ID , sample: 101010200 eg**/


/**
 *  @function  HttpGet
 *  @brief  Http Get Request
 *  @para   url  std::string
 *  @return:
 *
 **/
static std::string HttpGet(std::string url)
{
    std::string content;
    char *buf = new char[5120];

    /*set http timeout 8 seconds*/
    http_setTimeout(Default_HttpGet_TimeOut);

    //http Get request
    if (http_fetch((const char *)url.c_str(), &buf) == -1) {
        cout << "error request." << endl;
        WEATHER_LOG_WARNING("URL[%s] http request error!\n", url.c_str());
        std::string errStr = http_strerror();
        if (errStr.compare("Success") != 0) {
            http_perror( const_cast<char *>(errStr.c_str()));
            cout << errStr << endl;
            WEATHER_LOG_WARNING("URL[%s] error code[%s].\n", url.c_str(), errStr.c_str());
        }
        content = "ERROR_HTTP_REQUEST";
    } else {
        content = buf;
    }
    delete [] buf;
    
    WEATHER_LOG("Http Got Info [%s] End.\n", content.c_str());
    return std::string(content);
}



/**
 *  @class WeatherForecast
 *  @brief get weather forecast info, only [the same day] :
 *          city / city_en
 *          temp
 *          weather
 *          dayImgNo / nightImgNo
 *
 **/
WeatherForecast:: WeatherForecast(std::string cityCodeURL)
{
    std::string content;
    std::string urls = cityCodeURL;   //直接自动获取city ID

    content = HttpGet(urls);

    //TODO: get id code
    if (content.compare("ERROR_HTTP_REQUEST") != 0) {
        size_t pos = content.find("id=");
        cityCode = content.substr(pos + 3, LengthOfCityID);
        isErrorStatus = isErrorStatus | 6; // 6 = "110"
        g_cityID = cityCode;
    } else {
        // 获取地理位置失败
        if (isErrorStatus & 1) {
            cityCode = Default_CityID;
            g_cityID = cityCode;
        } else {  //从aqi中读取
            cityCode = g_cityID;
            isErrorStatus = isErrorStatus | 1;
        }
    }

    WEATHER_LOG("WeatherForecast::CityCode code: [%s].\n", cityCode.c_str());
}

WeatherForecast:: ~WeatherForecast()
{
    WEATHER_LOG("~WeatherForecast\n");
}

/**  flag: 0-city name in English; 1-city name in Chinese  **/
std::string WeatherForecast:: GetCity(int flag)
{
    if (flag == 1)  ///城市名中文
        return city;
    else if (flag == 0)  ///城市名英文
        return city_en;
    else
        return "";
}
std::string WeatherForecast:: SetCity(std::string c, int flag)
{
    if (flag == 1)  ///城市名中文
        city = c;
    else if (flag == 0)  ///城市名英文
        city_en = c;
    else
        return "";
    return c;
}

/**  flag: 0-night image; 1-day image  **/
ImgNO_ WeatherForecast:: GetImgNo(int flag)
{
    if (flag == 1)  ///flag: 0-night image;
        return dayImgNo;
    else if (flag == 0)  ///flag: 1-day image;
        return nightImgNo;
    else
        return ImgNO_UnDef;
}
ImgNO_ WeatherForecast:: SetImgNo(ImgNO_ i, int flag)
{
    if (flag == 1)  ///flag: 0-night image;
        return dayImgNo = i;
    else if (flag == 0)  ///flag: 1-day image;
        return nightImgNo = i;
    else
        return ImgNO_UnDef;
}

/**  Combine URL for Weather Forecast **/
void WeatherForecast:: SetURL(std::string urlParten)
{
    std::string urlPart;
    std::string infoURL = urlParten;
    if (infoURL.compare("") == 0)
        infoURL = WeatherInfoURL_CountryCenter;  //default url is Contury weather Center;
    WEATHER_LOG("infoURL: [%s].\n", infoURL.c_str());
    
    int pos = infoURL.find("{cityID}");
    url = infoURL.substr(0, pos);
    urlPart = infoURL.substr(pos + 8, infoURL.length() - pos - 8);
    url.append(cityCode);
    url.append(urlPart);
    WEATHER_LOG("The WeatherForecast main url: [%s].\n", url.c_str());
    
    /// set back url;
    infoURL = WeatherInfoBACKURL_CountryCenter;   //default url is Contury weather Center;
    pos = infoURL.find("{cityID}");
    urlBack = infoURL.substr(0, pos);
    urlPart = infoURL.substr(pos + 8, infoURL.length() - pos - 8);
    urlBack.append(cityCode);
    urlBack.append(urlPart);
    WEATHER_LOG("The WeatherForecast back url: [%s].\n", urlBack.c_str());
    
    return ;
}

/**  Get Today Temperature/Weather/imgNumber eg., example: "temp1": "12℃~-2℃" **/
std::string WeatherForecast:: GetInfo()
{
    std::string content;
    std::string retValue;
    Json::Value rValue;
    Json::Reader reader;

    content = HttpGet(url);
    if (content.compare("ERROR_HTTP_REQUEST") == 0 || content.find("<html>") != string::npos) {
        WEATHER_LOG_WARNING("Use urlBack to get Weather Forecast.\n");
        content = HttpGet(urlBack);
        if (content.compare("ERROR_HTTP_REQUEST") == 0 || content.find("<html>") != string::npos) { //weather info get error
            WEATHER_LOG_WARNING("Weather Forecast url[%s] http request error.\n", urlBack.c_str());
            isErrorStatus = isErrorStatus | (1 << 1); // 2 = "0010"
            return "ERROR_REQUEST";
        }
    }
    WEATHER_LOG("The WeatherForecast Http Get Info content: [%s]\n", content.c_str());
    
    reader.parse(content, rValue);
    retValue = "";
    if ( !rValue["weatherinfo"].isNull()) {
        std::stringstream ssTmp;
        Json::Value str = rValue["weatherinfo"];

        city = str["city"].asString();
        retValue.append("city:[ ");
        retValue.append(city);

        city_en = str["city_en"].asString();
        retValue.append(" ]city_en:[ ");
        retValue.append(city_en);

        temp = str["temp1"].asString();
        retValue.append(" ]temp:[ ");
        retValue.append(temp);

        weather = str["weather1"].asString();
        retValue.append(" ]weather:[ ");
        retValue.append(weather);

        int tmp;
        ssTmp << str["img1"].asString();
        ssTmp >> tmp;
        dayImgNo = ImgNO_(tmp);
        retValue.append(" ]dayImgNo:[ ");
        retValue.append(str["img1"].asString());

        ssTmp.clear();
        ssTmp << str["img2"].asString();
        ssTmp >> tmp;
        nightImgNo = ImgNO_(tmp);
        retValue.append(" ]nightImgNo:[ ");
        retValue.append(str["img2"].asString());
        retValue.append(" ] ");
    }

    WEATHER_LOG("The WeatherForecast GetInfo : [%s] end.\n", retValue.c_str());
    return retValue;
}


/**
 *  @class AQIQuery
 *  @brief get AQI(air quality index) query info:
 *          aqi
 *          pm10
 *          pm25
 *          no2
 *          so2
 **/
AQIQuery:: AQIQuery(std::string cityCodeURL)
{
    std::string content;
    std::string urls = cityCodeURL;   //直接自动获取city ID

    content = HttpGet(urls);

    //TODO: get id code
    if (content.compare("ERROR_HTTP_REQUEST") != 0) {
        size_t pos = content.find("id=");
        cityCode = content.substr(pos + 3, LengthOfCityID);
        isErrorStatus = isErrorStatus | 6; // 6 = "0110"
        g_cityID = cityCode;
    } else {
        // 获取地理位置失败
        if (isErrorStatus & 1) {
            cityCode = Default_CityID;
            g_cityID = cityCode;
        } else {        //从aqi中读取
            cityCode = g_cityID;
            isErrorStatus = isErrorStatus | 1; // 1 = "0010"
        }
    }

    WEATHER_LOG("The AQIQuery cityCode is: [%s] end.\n", cityCode.c_str());
}

AQIQuery:: ~AQIQuery()
{
    WEATHER_LOG("~AQIQuery.");
}

/**  Combine URL for AQI Query **/
void AQIQuery:: SetURL(std::string urlParten)
{
    std::string urlPart;
    std::string queryURL = urlParten;
    if (queryURL.compare("") == 0)
        queryURL = AirQualityInfoURL_Xiaomi;  //default url is xiaomi tianqi;
    WEATHER_LOG("The AQIQuery queryURL is: [%s].\n", queryURL.c_str());

    int pos = queryURL.find("{cityID}");
    url = queryURL.substr(0, pos);
    urlPart = queryURL.substr(pos + 8, queryURL.length() - pos - 8);
    url.append(cityCode);
    url.append(urlPart);
    WEATHER_LOG("The AQIQuery main url is: [%s].\n", url.c_str());

    /// set back url;
    queryURL = AirQualityInfoBACKURL_Xiaomi;   //default url is xiaomi tianqi;
    pos = queryURL.find("{cityID}");
    urlBack = queryURL.substr(0, pos);
    urlPart = queryURL.substr(pos + 8, queryURL.length() - pos - 8);
    urlBack.append(cityCode);
    urlBack.append(urlPart);
    WEATHER_LOG("The AQIQuery back url is: [%s].\n", urlBack.c_str());

    return ;
}

std::string AQIQuery:: GetInfo()
{
    std::string content;
    std::string retValue;
    Json::Value rValue;
    Json::Reader reader;

    //TODO: get today aqi
    content = HttpGet(url);

    if (content.compare("ERROR_HTTP_REQUEST") == 0 || content.find("<html>") != string::npos) {
        WEATHER_LOG_WARNING("The AQIQuery use urlBack[%s].\n", urlBack.c_str());
        content = HttpGet(urlBack);
        if (content.compare("ERROR_HTTP_REQUEST") == 0 || content.find("<html>") != string::npos) {  //aqi info get error
            WEATHER_LOG_WARNING("The AQIQuery back url[%s] http request error.\n", urlBack.c_str());
            isErrorStatus = isErrorStatus | (1 << 2); // 4 = "0100"
            return "ERROR_REQUEST";
        }
    }
    cout << content << endl;
    WEATHER_LOG("The AQIQuery Get Info content[%s].\n", content.c_str());

    reader.parse(content, rValue);
    retValue = "";
    if (!rValue["aqi"].isNull()) {
        std::stringstream ssTmp;
        Json::Value str = rValue["aqi"];

        ssTmp << str["aqi"].asString();
        ssTmp >> aqi;
        retValue.append("aqi:[ ");
        retValue.append(str["aqi"].asString());

        ssTmp.clear(); //在进行多次转换前，必须清除stream
        ssTmp << str["pm10"].asString();
        ssTmp >> pm10;
        retValue.append(" ]pm10:[ ");
        retValue.append(str["pm10"].asString());

        ssTmp.clear();
        ssTmp << str["pm25"].asString();
        ssTmp >> pm25;
        retValue.append(" ]pm25:[ ");
        retValue.append(str["pm25"].asString());

        ssTmp.clear();
        ssTmp << str["no2"].asString();
        ssTmp >> no2;
        retValue.append(" ]no2:[ ");
        retValue.append(str["no2"].asString());

        ssTmp.clear();
        ssTmp << str["so2"].asString();
        ssTmp >> so2;
        retValue.append(" ]so2:[ ");
        retValue.append(str["so2"].asString());
        retValue.append(" ] ");
    }

    WEATHER_LOG("The AQIQuery GetInfo retValue: [%s] end.\n", retValue.c_str());
    return retValue;
}


/**
 *  @class  Weather
 *  @brief  Open interface, Package the weather forecast and AQI interface
 *
 **/
Weather:: Weather(std::string cityCodeURL)
    : WeatherForecast(cityCodeURL), AQIQuery(cityCodeURL)
{
    WEATHER_LOG("Weather:: Weather\n");
}

Weather:: ~Weather()
{
    WEATHER_LOG("Weather:: ~Weather\n");
}

int Weather:: GetWeatherInfo(std::string weatherUrlParten, std::string aqiUrlParten)
{
    std::string contentWeather, contentAQI, content;
    std::string errCode;
    
    WEATHER_LOG("The WeatherForecast URL Parten[%s], The AQIQuery URL Parten[%s].\n", weatherUrlParten.c_str(), aqiUrlParten.c_str());
    WeatherForecast:: SetURL( weatherUrlParten);
    AQIQuery:: SetURL( aqiUrlParten);
    WEATHER_LOG("The WeatherForecast request URL[%s], The AQIQuery query URL[%s].\n", WeatherForecast::GetURL().c_str(), AQIQuery::GetURL().c_str());

    contentWeather = WeatherForecast:: GetInfo();
    contentAQI = AQIQuery:: GetInfo();
    content.append(contentWeather);
    content.append(contentAQI);
    
    WEATHER_LOG("GetWeatherInfo: [%s], isErrorStatus[%d] end.\n", content.c_str(), isErrorStatus);
    return isErrorStatus;
}



