
class AsyncSDFileResponse: public AsyncAbstractResponse {
  private:
    File _content;
    String _path;
    void _setContentType(const String& path);
    bool _sourceIsValid;
  public:
    AsyncSDFileResponse(fs::FS &fs, const String& path, const String& contentType=String(), bool download=false);
    AsyncSDFileResponse(File content, const String& path, const String& contentType=String(), bool download=false);
    ~AsyncSDFileResponse();
    bool _sourceValid() const { return _sourceIsValid; } 
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;
};

class AsyncStaticSDWebHandler: public AsyncWebHandler {
  private:
    bool _getFile(AsyncWebServerRequest *request);
    bool _fileExists(AsyncWebServerRequest *request, const String& path);
    uint8_t _countBits(const uint8_t value) const;
  protected:
    fs::FS _fs;
    String _uri;
    String _path;
    String _default_file;
    String _cache_control;
    String _last_modified;
    bool _isDir;
    bool _gzipFirst;
    uint8_t _gzipStats;
  public:
    AsyncStaticSDWebHandler(const char* uri, fs::FS& fs, const char* path, const char* cache_control = NULL);
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    AsyncStaticSDWebHandler& setIsDir(bool isDir);
    AsyncStaticSDWebHandler& setDefaultFile(const char* filename);
    AsyncStaticSDWebHandler& setCacheControl(const char* cache_control);
    AsyncStaticSDWebHandler& setLastModified(const char* last_modified);
    AsyncStaticSDWebHandler& setLastModified(struct tm* last_modified);
  #ifdef ESP8266
    AsyncStaticSDWebHandler& setLastModified(time_t last_modified);
    AsyncStaticSDWebHandler& setLastModified(); //sets to current time. Make sure sntp is runing and time is updated
  #endif
};

