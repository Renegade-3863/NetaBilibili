#pragma once
#define _GNU_SOURCE
#include "HttpRequest.h"
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define HeaderSize 20
#define MAX_FILE_SIZE (5 * 1024 * 1024) // 5MB
#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct HttpRequest *httpRequestInit()
{
    struct HttpRequest *request = (struct HttpRequest *)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader *)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}

void httpRequestReset(struct HttpRequest *req)
{
    req->curState = PS_REQLINE;
    req->method = NULL;
    req->url = NULL;
    req->version = NULL;
    req->reqHeadersNum = 0;
}

void httpRequestResetEx(struct HttpRequest *req)
{
    free(req->url);
    free(req->method);
    free(req->version);
    if (req->reqHeaders)
    {
        for (int i = 0; i < req->reqHeadersNum; ++i)
        {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
    httpRequestReset(req);
}

void httpRequestDestroy(struct HttpRequest *req)
{
    if (req)
    {
        httpRequestResetEx(req);
        free(req);
    }
}

ParseState HttpRequestState(struct HttpRequest *request)
{
    return request->curState;
}

void httpRequestAddHeader(struct HttpRequest *request, const char *key, const char *value)
{
    request->reqHeaders[request->reqHeadersNum].key = (char *)key;
    request->reqHeaders[request->reqHeadersNum].value = (char *)value;
    request->reqHeadersNum++;
}

char *httpRequestGetHeader(struct HttpRequest *request, const char *key)
{
    if (request)
    {
        for (int i = 0; i < request->reqHeadersNum; ++i)
        {
            if (strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0)
            {
                return request->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}

static char *splitRequestLine(char *start, char *end, const char *sub, char **ptr)
{
    char *space = end;
    if (sub != NULL)
    {
        space = memmem(start, end - start, sub, strlen(sub));
        assert(space);
    }
    int length = space - start;
    char *tmp = (char *)malloc(length + 1);
    strncpy(tmp, start, length);
    tmp[length] = '\0';
    *ptr = tmp;
    return space + 1;
}

// --------------------- helpers for modular parsing/writing ---------------------

// 清理上一次请求解析时分配的 method/url/version 和 headers 字符串，保留 headers 数组容量
static inline void request_cleanup_previous_allocs(struct HttpRequest *request)
{
    if (request->method)
    {
        free(request->method);
        request->method = NULL;
    }
    if (request->url)
    {
        free(request->url);
        request->url = NULL;
    }
    if (request->version)
    {
        free(request->version);
        request->version = NULL;
    }
    if (request->reqHeaders)
    {
        for (int i = 0; i < request->reqHeadersNum; ++i)
        {
            free(request->reqHeaders[i].key);
            free(request->reqHeaders[i].value);
        }
        request->reqHeadersNum = 0;
    }
}

// 将新上传的视频条目写入 videos.json（位于 static/SimpleHttp/public/videos.json）
// 采用简单的读-改-写：
// - 若文件不存在则创建为 [ newItem ]
// - 若已存在且包含相同文件路径则不重复添加
// - 否则在结尾 ] 前追加 , newItem
static void append_video_to_videos_json(const char *filename, const char *url_path, int duration_sec)
{
    // 写入到站点的 public 目录（相对当前工作目录，main.c 会 chdir 到站点根）
    const char *json_path = "public/videos.json";
    const char *tmp_path = "public/videos.json.tmp";

    // id/title 使用去扩展名的名称
    char base[128] = {0};
    snprintf(base, sizeof(base), "%s", filename);
    // 去掉扩展名
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
    {
        if (base[i] == '.')
        {
            base[i] = '\0';
            break;
        }
        if (base[i] == '/' || base[i] == '\\')
            break;
    }

    // 构造下载链接
    const char *dl_prefix = "http://localhost:8080/upload/";
    size_t dl_len = strlen(dl_prefix) + strlen(filename) + 1;
    char *download_url = (char *)malloc(dl_len);
    if (!download_url)
        return;
    snprintf(download_url, dl_len, "%s%s", dl_prefix, filename);

    // 构造 JSON 条目（使用动态缓冲避免被长文件名截断导致 JSON 失配）
    const char *tmpl = "{\n  \"id\": \"%s\",\n  \"title\": \"%s\",\n  \"file\": \"%s\",\n  \"poster\": \"\",\n  \"download\": \"%s\",\n  \"duration\": %d\n}";
    size_t need = (size_t)snprintf(NULL, 0, tmpl, base, base, url_path, download_url, duration_sec) + 1;
    char *newItem = (char *)malloc(need);
    if (!newItem)
    {
        free(download_url);
        return;
    }
    snprintf(newItem, need, tmpl, base, base, url_path, download_url, duration_sec);

    // 读入原文件内容
    // 确保 public 目录存在
    struct stat st_pub;
    if (stat("public", &st_pub) == -1)
    {
        mkdir("public", 0755);
    }

    FILE *fp = fopen(json_path, "rb");
    printf("Reading JSON file: %s\n", json_path);
    if (!fp)
    {
        // 直接创建一个新文件
        printf("Creating new JSON file: %s\n", json_path);
        FILE *out = fopen(json_path, "wb");
        if (!out)
        {
            free(newItem);
            free(download_url);
            return;
        }
        fprintf(out, "[\n%s\n]\n", newItem);
        fclose(out);
        free(newItem);
        free(download_url);
        return;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len < 0)
    {
        fclose(fp);
        return;
    }
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf)
    {
        fclose(fp);
        free(newItem);
        free(download_url);
        return;
    }
    size_t nread = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    buf[nread] = '\0';

    // 如果已经包含相同 url_path，则跳过
    if (strstr(buf, url_path) != NULL)
    {
        free(buf);
        free(newItem);
        free(download_url);
        return;
    }

    // 找到最后一个 ']' 位置
    long end = (long)strlen(buf) - 1;
    while (end >= 0 && (buf[end] == '\n' || buf[end] == '\r' || buf[end] == ' ' || buf[end] == '\t'))
        end--;
    // 如果找不到有效的 ']'，则重置为一个空数组
    int hasItems = 0;
    long rbracket = -1;
    for (long i = end; i >= 0; --i)
    {
        if (buf[i] == ']')
        {
            rbracket = i;
            break;
        }
    }
    if (rbracket < 0)
    {
        free(buf);
        FILE *out = fopen(json_path, "wb");
        if (!out)
        {
            free(newItem);
            free(download_url);
            return;
        }
        fprintf(out, "[\n%s\n]\n", newItem);
        fclose(out);
        free(newItem);
        free(download_url);
        return;
    }
    // 判断当前数组是否非空：在 [ 和 ] 之间是否存在 '{'
    for (long i = 0; i < rbracket; ++i)
    {
        if (buf[i] == '{')
        {
            hasItems = 1;
            break;
        }
    }

    // 输出到临时文件
    FILE *out = fopen(tmp_path, "wb");
    if (!out)
    {
        free(buf);
        free(newItem);
        free(download_url);
        return;
    }
    // 写入到 ']' 前的所有内容（去掉末尾的空白再回退到 ']' 之前）
    fwrite(buf, 1, (size_t)rbracket, out);
    // 插入逗号分隔
    if (hasItems)
    {
        fprintf(out, ",\n%s\n]", newItem);
    }
    else
    {
        fprintf(out, "\n%s\n]", newItem);
    }
    // 追加换行
    fprintf(out, "\n");
    fclose(out);
    free(buf);
    free(newItem);
    free(download_url);

    // 原子替换
    rename(tmp_path, json_path);
}

// 在缓冲区中查找 CRLFCRLF，返回指向 "\r\n\r\n" 起始位置的指针，找不到返回 NULL
static inline char *find_headers_terminator(char *start, int readable)
{
    return memmem(start, readable, "\r\n\r\n", 4);
}

// 解析请求行，填充 request->method/url/version
static inline bool parse_start_line(struct HttpRequest *request, char *start, size_t header_block_len, char **line_end_out)
{
    char *line_end = memmem(start, header_block_len, "\r\n", 2);
    if (!line_end)
    {
        printf("parseHttpRequest: Invalid request line, no CRLF found\n");
        return false;
    }
    char *after = splitRequestLine(start, line_end, " ", &request->method);
    if (!after)
    {
        printf("parseHttpRequest: Failed to parse method\n");
        return false;
    }
    after = splitRequestLine(after, line_end, " ", &request->url);
    if (!after)
    {
        printf("parseHttpRequest: Failed to parse URL\n");
        return false;
    }
    char *q = strchr(request->url, '?');
    if (q)
        *q = '\0';
    char *tmpver = NULL;
    splitRequestLine(after, line_end, NULL, &tmpver);
    request->version = tmpver;
    if (line_end_out)
        *line_end_out = line_end;
    return true;
}

// 解析 header 行到 request->reqHeaders 数组
static inline bool parse_headers_block(struct HttpRequest *request, char *headers_start, char *hdr_end)
{
    request->reqHeadersNum = 0;
    char *hp = headers_start;
    while (hp < hdr_end)
    {
        char *next_eol = memmem(hp, hdr_end - hp, "\r\n", 2);
        if (!next_eol)
        {
            printf("parseHttpRequest: Invalid header format, no CRLF found\n");
            return false;
        }
        size_t line_len = (size_t)(next_eol - hp);
        if (line_len == 0)
        {
            break; // 空行，header 结束
        }
        char *colon = memmem(hp, line_len, ": ", 2);
        if (!colon)
        {
            printf("parseHttpRequest: Invalid header format, no colon found\n");
            return false;
        }
        size_t key_len = (size_t)(colon - hp);
        size_t val_len = line_len - key_len - 2; // 去掉": "
        char *key = (char *)malloc(key_len + 1);
        char *val = (char *)malloc(val_len + 1);
        if (!key || !val)
        {
            free(key);
            free(val);
            printf("parseHttpRequest: Failed to allocate memory for headers\n");
            return false;
        }
        memcpy(key, hp, key_len);
        key[key_len] = '\0';
        memcpy(val, colon + 2, val_len);
        val[val_len] = '\0';
        httpRequestAddHeader(request, key, val);
        hp = next_eol + 2;
    }
    return true;
}

// 读取 Content-Length / Transfer-Encoding: chunked 标志
static inline void get_body_meta(struct HttpRequest *request, long *content_length_out, bool *is_chunked_out)
{
    long content_length = -1;
    bool is_chunked = false;
    char *cl = httpRequestGetHeader(request, "Content-Length");
    char *te = httpRequestGetHeader(request, "Transfer-Encoding");
    if (cl)
    {
        content_length = atol(cl);
        if (content_length < 0) content_length = -1;
    }
    if (te && strcasestr(te, "chunked") != NULL)
    {
        is_chunked = true;
    }
    if (content_length_out) *content_length_out = content_length;
    if (is_chunked_out) *is_chunked_out = is_chunked;
}

// 继续处理已存在的上传：把 readBuf 中的数据尽量写入，必要时完成响应
static inline ParseResult upload_continue_write(struct TcpConnection *conn,
                                                struct Buffer *readBuf,
                                                struct HttpResponse *response,
                                                struct Buffer *sendBuf,
                                                int socket)
{
    char *base = readBuf->data;
    int available = bufferReadableSize(readBuf);
    if (available <= 0)
        return PARSE_INCOMPLETE;

    ssize_t remaining = conn->upload->expected >= 0 ? (conn->upload->expected - conn->upload->received) : available;
    int to_write = (int)((remaining < available && conn->upload->expected >= 0) ? remaining : available);
    int written_total = 0;
    while (written_total < to_write)
    {
        ssize_t n = write(conn->upload->fd, base + readBuf->readPos + written_total, to_write - written_total);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            char path_saved[256];
            snprintf(path_saved, sizeof(path_saved), "%s", conn->upload->path);
            close(conn->upload->fd);
            unlink(path_saved);
            free(conn->upload);
            conn->upload = NULL;
            readBuf->readPos += written_total;
            httpResponseSendJson(conn, response, sendBuf, socket, InternalServerError, "{\"error\":\"write failed\"}");
            return PARSE_OK;
        }
        if (n == 0) break;
        written_total += n;
    }
    conn->upload->received += written_total;
    readBuf->readPos += written_total;

    if (conn->upload->expected >= 0 && conn->upload->received < conn->upload->expected)
        return PARSE_INCOMPLETE;

    // 完成
    close(conn->upload->fd);
    conn->upload->fd = -1;
    char json[256];
    snprintf(json, sizeof(json), "{\"ok\":true,\"path\":\"/static/uploads/%s\",\"size\":%ld}", conn->upload->filename, (long)conn->upload->received);
    char url_path[256];
    snprintf(url_path, sizeof(url_path), "/static/uploads/%s", conn->upload->filename);
    append_video_to_videos_json(conn->upload->filename, url_path, conn->upload->duration_sec);
    httpResponseSendJson(conn, response, sendBuf, socket, Created, json);
    free(conn->upload);
    conn->upload = NULL;
    return PARSE_OK;
}

// 初始化上传上下文：解码文件名、创建目录、打开文件、记录期望长度和时长
static inline bool upload_init_from_request(struct TcpConnection *conn,
                                            struct HttpRequest *request,
                                            long content_length)
{
    if (conn->upload != NULL) return true;

    const char *encoded = request->url + 8; // "/upload/"
    char decoded_name[256] = {0};
    decodeMsg(decoded_name, (char *)encoded);
    if (decoded_name[0] == '\0')
    {
        return false;
    }
    struct stat st_dir;
    if (stat("static/uploads", &st_dir) == -1)
    {
        mkdir("static/uploads", 0755);
    }
    char path[256];
    snprintf(path, sizeof(path), "static/uploads/%s", decoded_name);
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd == -1)
    {
        return false;
    }
    conn->upload = (struct UploadCtx *)calloc(1, sizeof(struct UploadCtx));
    conn->upload->fd = fd;
    conn->upload->expected = content_length;
    conn->upload->received = 0;
    conn->upload->chunked = false;
    snprintf(conn->upload->filename, sizeof(conn->upload->filename), "%s", decoded_name);
    snprintf(conn->upload->path, sizeof(conn->upload->path), "%s", path);
    int duration_sec = 0;
    char *xdur = httpRequestGetHeader(request, "X-Video-Duration");
    if (xdur)
    {
        long v = strtol(xdur, NULL, 10);
        if (v < 0) v = 0;
        if (v > 24 * 60 * 60) v = 24 * 60 * 60;
        duration_sec = (int)v;
    }
    conn->upload->duration_sec = duration_sec;
    return true;
}

// 处理带 Content-Length 的 /upload 路由（首段+后续段）
static inline ParseResult handle_upload_with_content_length(struct TcpConnection *conn,
                                                           struct HttpRequest *request,
                                                           struct Buffer *readBuf,
                                                           struct HttpResponse *response,
                                                           struct Buffer *sendBuf,
                                                           int socket,
                                                           char *base,
                                                           int body_start_pos,
                                                           int total_readable_after_headers,
                                                           size_t header_block_len,
                                                           long content_length)
{
    // 初始化 upload 上下文（仅首段时生效）
    if (conn->upload == NULL)
    {
        if (!upload_init_from_request(conn, request, content_length))
        {
            // 至少消费 header，避免重复解析
            readBuf->readPos += (int)header_block_len;
            httpResponseSendJson(conn, response, sendBuf, socket, BadRequest, "{\"error\":\"invalid filename or open failed\"}");
            return PARSE_OK;
        }
    }

    // 写入本批 body 字节
    int available = total_readable_after_headers;
    if (available < 0) available = 0;
    const char *body_ptr = base + body_start_pos;
    int to_consume = available;
    if (conn->upload->expected >= 0)
    {
        ssize_t remaining = conn->upload->expected - conn->upload->received;
        if (remaining < to_consume) to_consume = (int)remaining;
    }
    int written_total = 0;
    while (written_total < to_consume)
    {
        ssize_t n = write(conn->upload->fd, body_ptr + written_total, to_consume - written_total);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            char path_saved[256];
            snprintf(path_saved, sizeof(path_saved), "%s", conn->upload->path);
            close(conn->upload->fd);
            unlink(path_saved);
            free(conn->upload);
            conn->upload = NULL;
            readBuf->readPos += (int)header_block_len + written_total;
            httpResponseSendJson(conn, response, sendBuf, socket, InternalServerError, "{\"error\":\"write failed\"}");
            return PARSE_OK;
        }
        if (n == 0) break;
        written_total += n;
    }
    conn->upload->received += written_total;
    readBuf->readPos += (int)header_block_len + written_total;

    if (conn->upload->received < conn->upload->expected)
        return PARSE_INCOMPLETE;

    // 完成：关闭并发送 201
    close(conn->upload->fd);
    conn->upload->fd = -1;
    char json[256];
    snprintf(json, sizeof(json), "{\"ok\":true,\"path\":\"/static/uploads/%s\",\"size\":%ld}", conn->upload->filename, (long)conn->upload->expected);
    char url_path[256];
    snprintf(url_path, sizeof(url_path), "/static/uploads/%s", conn->upload->filename);
    append_video_to_videos_json(conn->upload->filename, url_path, conn->upload->duration_sec);
    httpResponseSendJson(conn, response, sendBuf, socket, Created, json);
    free(conn->upload);
    conn->upload = NULL;
    request->curState = PS_REQLINE;
    return PARSE_OK;
}

// 非上传请求：等待完整 body，再交由业务处理
static inline ParseResult handle_regular_with_body(struct TcpConnection *conn,
                                                   struct HttpRequest *request,
                                                   struct Buffer *readBuf,
                                                   struct HttpResponse *response,
                                                   struct Buffer *sendBuf,
                                                   int socket,
                                                   size_t header_block_len,
                                                   long content_length)
{
    int total_readable_after_headers = bufferReadableSize(readBuf) - (int)header_block_len;
    if (total_readable_after_headers < content_length)
        return PARSE_INCOMPLETE;
    int consume_len = (int)header_block_len + (int)content_length;
    readBuf->readPos += consume_len;
    processHttpRequest(conn, request, response);
    httpResponsePrepareMsg(conn, response, sendBuf, socket);
    request->curState = PS_REQLINE;
    return PARSE_OK;
}

// 判断是否为上传路由：仅支持 PUT/POST /upload/<filename>
static bool is_upload_route(const char *method, const char *url)
{
    if (!method || !url)
        return false;
    if (strncasecmp(method, "put", 3) != 0 && strncasecmp(method, "post", 4) != 0)
        return false;
    // 判断路径是否以 /upload/ 开头，同时存在一个有名字的文件
    return strncmp(url, "/upload/", 8) == 0 && strlen(url) > 8;
}

// --------------------- End of helpers for modular parsing/writing ---------------------

/**
 * @return PARSE_INCOMPLETE: 当前 缓冲中的数据不足以完成下一步，调用者保留缓冲区，不关闭连接，等待下次可读
 * @return PARSE_OK: 解析成功
 * @return PARSE_ERROR: 解析出错，调用者准备发送 400 并关闭连接
 */
ParseResult parseHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct Buffer *readBuf, struct HttpResponse *response, struct Buffer *sendBuf, int socket)
{
    // 释放上一次解析的分配项，避免复用连接导致的泄漏
    request_cleanup_previous_allocs(request);

    // 快速别名？
    char *base = readBuf->data;
    int readable = bufferReadableSize(readBuf);
    // 解析不了，就返回 PARSE_INCOMPLETE
    if (readable <= 0)
    {
        return PARSE_INCOMPLETE;
    }

    // 若已有正在进行的上传（已消费完 headers，仅余/新增 body），直接继续写入文件
    if (conn->upload != NULL)
    {
        ParseResult r = upload_continue_write(conn, readBuf, response, sendBuf, socket);
        if (r != PARSE_INCOMPLETE)
            request->curState = PS_REQLINE;
        return r;
    }

    // 1) 找到 headers 结束标记 "\r\n\r\n"
    char *hdr_end = find_headers_terminator(base + readBuf->readPos, readable);
    if (!hdr_end)
    {
        // header 未完整到达
        return PARSE_INCOMPLETE;
    }

    // header block 范围 [start, hdr_end + 4]，也就是函数头的部分
    char *start = base + readBuf->readPos;
    size_t header_block_len = (hdr_end + 4) - start;
    hdr_end += 4;
    // 2) 解析 request line （第一行）：method SP url SP version CRLF
    char *line_end = NULL;
    if (!parse_start_line(request, start, header_block_len, &line_end))
        return PARSE_ERROR;

    // 3) 解析 headers 部分（从 line_end + 2 到 hdr_end）
    // 先清理旧的 headers
    if (!parse_headers_block(request, line_end + 2, hdr_end))
        return PARSE_ERROR;

    // 4) 决定是否有 body (检查 Content-Length 或 Transfer-Encoding: chunked)
    long content_length = -1; bool is_chunked = false;
    get_body_meta(request, &content_length, &is_chunked);

    // 定位 body 的起始位置（如果 header_block_len > 0，则 body_start = readPos + header_block_len)
    int body_start_pos = readBuf->readPos + header_block_len;
    // 判断 body 总共有多少可读的数据
    int total_readable_after_headers = bufferReadableSize(readBuf) - (int)header_block_len;

    if (total_readable_after_headers < 0)
    {
        total_readable_after_headers = 0;
    }

    // ——— 异步接收 POST: 支持基于 Content-Length 的流式写入 ———
    if (content_length >= 0)
    {
        // 上传路由：异步流式落盘
        if (is_upload_route(request->method, request->url))
        {
            return handle_upload_with_content_length(conn, request, readBuf, response, sendBuf, socket,
                                                     base, body_start_pos, total_readable_after_headers,
                                                     header_block_len, content_length);
        }

        // 非上传请求，继续保持原行为：必须等到完整 body 才处理
        return handle_regular_with_body(conn, request, readBuf, response, sendBuf, socket, header_block_len, content_length);
    }
    else
    {
        // 没有 body，也就是 (GET/HEAD 等方法)：只消费 header_block
        readBuf->readPos += (int)header_block_len;
        // 调用业务处理并准备响应
        processHttpRequest(conn, request, response);
        httpResponsePrepareMsg(conn, response, sendBuf, socket);
        request->curState = PS_REQLINE; // 重置状态以便下次
        return PARSE_OK;
    }

    // make the compiler happy
    printf("parseHttpRequest: Unexpected state\n");
    return PARSE_ERROR;
}

/*
    这是一个 static 函数，用于将 Linux 文件系统中的字节流转换为 UTF-8 编码
    主要用于处理上传的文件内容
*/

/**
 * @param c 16 进制字符
 */
// 把 16 进制字符转换为 10 进制数字
static int hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

/**
 * @param filename 要部分发送的文件名
 * @param sendBuf 发送缓冲
 * @param cfd 发送的文件描述符
 * @param offset 要发送的文件偏移量
 * @param length 要发送的文件长度
 *
 */
static void sendFilePartial(const char *filename, struct Buffer *sendBuf,
                            int cfd, int offset, int length)
{
    int fd = open(filename, O_RDONLY);
    assert(fd > 0);

    off_t off = offset;
    int remaining = length;

    while (remaining > 0)
    {
        // 一次最多发送 65536 字节
        size_t toSend = remaining > 65536 ? 65536 : remaining;
        ssize_t sent = sendfile(cfd, fd, &off, toSend);
        if (sent <= 0)
        {
            if (errno == EINTR)
            {
                // 被信号中断，重试
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 非阻塞模式下没有数据可发送，退出循环，等待下一次写事件
                break;
            }
            else
            {
                // 其他错误，打印错误信息，关闭文件描述符，返回

                perror("sendfile");
                break;
            }
        }
        remaining -= sent;
    }
    close(fd);
}
/**
 * @param to 目标字符串
 * @param from 源字符串
 */
// 解码函数
void decodeMsg(char *to, char *from)
{
    // 逐个字符进行解码
    for (; *from != '\0'; ++to, ++from)
    {
        // isxdigit 函数用于判断字符是否为16进制字符
        // 将 UTF-8 编码的文件名进行解码
        // Linux%E5%86%85%E6%A0%B8.jpg
        // % 后面跟着 2 个 16 进制字符
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // % 后面跟着 2 个 16 进制字符，说明 from 当前指向的字符串是一个 UTF-8 编码的字符串
            // 需要将其转换为 10 进制数字
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

            // 转换为 10 进制数字后，将其赋值给 to 指向的字符
            // 然后将 to 指针和 from 指针都向后移动 2 个位置
            // 因为 % 后面跟着 2 个 16 进制字符，所以需要移动 2 个位置
            from += 2;
        }
        else
        {
            // 否则，说明 from 当前指向的字符是一个 ASCII 字符
            // 直接将其赋值给 to 指向的字符
            *to = *from;
        }
    }
    // 最后，在目标字符串的末尾添加一个 '\0' 字符，作为字符串的结束标志
    *to = '\0';
}

const char *getFileType(const char *name)
{
    // a.jpg a.mp4 a.html
    // 查找最后一个 '.' 字符的位置
    // 如果找不到，说明是普通文件，默认返回 text/plain
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8"; // ���ı�
    if (strcmp(dot, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    if (strcmp(dot, ".mp4") == 0)
        return "video/mp4";
    if (strcmp(dot, ".json") == 0)
        return "application/json";
    if (strcmp(dot, ".js") == 0)
        return "text/javascript; charset=utf-8";
    return "text/plain; charset=utf-8";
}

// 这个函数仅用于测试目录文档，实际使用中应该不会用到
void sendDir(const char *dirName, struct Buffer *sendBuf, int cfd)
{
    char buf[4096] = {0};
    // 构造 html 文件的 head 部分
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
    struct dirent **namelist;
    // 这里需要进行过滤，防止出现 NULL
    // 遍历目录时默认使用 alphasort 形式对文件进行字母排序
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for (int i = 0; i < num; ++i)
    {
        // 取出一个文件名
        // 通过 namelist 指向的一个指针变量 struct dirent *tmp
        char *name = namelist[i]->d_name;
        // 判断当前文件名是否为一个目录或一个普通文件
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName, name);
        stat(subPath, &st);
        if (S_ISDIR(st.st_mode))
        {
            // 如果是目录，则使用 <a></a> 标签
            // 结构为 <a href="">name</a>
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        else
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        // send(cfd, buf, strlen(buf), 0);
        bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
        // 写一点，就发送一点
        bufferSendData(sendBuf, cfd);
#endif
        // 清空缓冲区
        memset(buf, 0, sizeof(buf));
        // 释放内存
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
    // дһ���֣��ͷ�һ����
    bufferSendData(sendBuf, cfd);
#endif
    // 释放内存
    free(namelist);
}

// 目前这个函数只用于发送 404.html，其它较大文件使用 sendRangeRequestData 来做
// 由于循环发送的方式太过低效，后面可能会考虑删掉
void sendFile(const char *filename, struct Buffer *sendBuf, int cfd)
{
    // 打开文件
    int fd = open(filename, O_RDONLY);
    // 打开失败
    if (fd <= 0)
    {
        perror("open");
        return;
    }
#if 1
    while (1)
    {
        // 后续可能会调整
        char buf[1024];
        int len = read(fd, buf, sizeof(buf));
        if (len > 0)
        {
            int rc_append = bufferAppendData(sendBuf, buf, len);
            if (rc_append != 0)
            {
#ifndef MSG_SEND_AUTO
                int rc_send = bufferSendData(sendBuf, cfd);
                if (rc_send == -1)
                {
                    // 发生了致命错误，例如 broken pipe (EPIPE)，此时连接不再可用，我们直接关闭文件描述符即可
                    // 不再重复尝试进行发送
                    close(fd);
                    return;
                }
#endif
            }
        }
        else if (len == 0)
        {
            // 读取完毕，退出循环
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
    }
#else
    off_t offset = 0;
    // 获取文件大小
    int size = lseek(fd, 0, SEEK_END);
    // 设置一个读写指针，指向 fd 文件的开头
    lseek(fd, 0, SEEK_SET);
    // 通过 sendfile 进行数据传输
    // 设定一个发送限速
    while (offset < size)
    {
        // 注意 sendfile 需要发送的数据块大小必须是一个完整的 offset 值和 size 之间的区间
        // 给 sendfile 设定一个发送限速
        int ret = sendfile(cfd, fd, &offset, size - offset);
        // 因为是 ET 模式，需要注意处理 EAGAIN 和 EWOULDBLOCK
        if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("暂时无法发送数据...\n");
            // 直接返回即可
            return 0;
        }
        if (ret == -1 && errno != EAGAIN)
        {
            // 发生了致命错误，关闭文件描述符并返回 -1
            close(fd);
            return -1;
        }
        offset += ret;
    }
#endif
    close(fd);
}

// 判断一个 HTTP 请求是否是 WebSocket 握手请求
static bool isWebSocketHandshake(struct HttpRequest *request)
{
    // 1. 检查是否为 WebSocket 握手请求
    const char *upgrade = httpRequestGetHeader(request, "Upgrade");
    const char *connection = httpRequestGetHeader(request, "Connection");
    const char *ws_key = httpRequestGetHeader(request, "Sec-WebSocket-Key");
    const char *ws_version = httpRequestGetHeader(request, "Sec-WebSocket-Version");

    if (upgrade && strcasecmp(upgrade, "websocket") == 0 &&
        connection && strcasecmp(connection, "Upgrade") == 0 &&
        ws_key && ws_version && strcmp(ws_version, "13") == 0)
    {
        // 表示这是一个 WebSocket 握手请求
        // 返回 true
        return true;
    }
    // 否则表示不是 WebSocket 握手请求
    return false;
}

static int base64Encode(const unsigned char *input, int inputLen, char *output)
{
    BIO *bmem = NULL, *b64 = NULL;
    BUF_MEM *bptr = NULL;

    // 创建一个内存 BIO
    b64 = BIO_new(BIO_f_base64());
    // 关闭换行符
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input, inputLen);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    memcpy(output, bptr->data, bptr->length);
    output[bptr->length] = '\0';

    BIO_free_all(b64);
    return bptr->length;
}

// 处理 WebSocket 握手请求
static bool handleWebSocketHandshake(struct HttpRequest *request, struct HttpResponse *response)
{
    const char *ws_key = httpRequestGetHeader(request, "Sec-WebSocket-Key");
    if (!ws_key)
    {
        // 缺少 Sec-WebSocket-Key 头部，说明不是一个合法的 WebSocket 握手请求
        return false;
    }
    // 是一个合法的 WebSocket 握手请求
    // 添加 Sec-WebSocket-Accept 响应头
    // 拼接 GUID
    char acceptKey[128] = {0};
    snprintf(acceptKey, sizeof(acceptKey), "%s%s", ws_key, WEBSOCKET_GUID);

    // 进行 SHA-1 计算（为什么不能用 SHA-256?）
    unsigned char hash[SHA_DIGEST_LENGTH] = {0};
    // 采用 SHA-1 算法对拼接后的字符串进行哈希
    SHA1((unsigned char *)acceptKey, strlen(acceptKey), hash);

    // 将哈希值进行 Base64 编码
    char base64Hash[128] = {0};
    int base64Len = base64Encode(hash, SHA_DIGEST_LENGTH, base64Hash);
    (void)base64Len;

    // 将 Base64 编码后的字符串添加到响应头中
    response->statusCode = SwitchingProtocols;
    snprintf(response->statusMsg, sizeof(response->statusMsg), "%s", "Switching Protocols");
    // 添加 Sec-WebSocket-Accept 响应头
    httpResponseAddHeader(response, "Sec-WebSocket-Accept", base64Hash);
    // 添加 Upgrade 响应头
    httpResponseAddHeader(response, "Upgrade", "websocket");
    // 添加 Connection 响应头
    httpResponseAddHeader(response, "Connection", "Upgrade");
    // 添加 Sec-WebSocket-Version 响应头
    httpResponseAddHeader(response, "Sec-WebSocket-Version", "13");
    // 添加 Sec-WebSocket-Protocol 响应头
    // httpResponseAddHeader(response, "Sec-WebSocket-Protocol", "chat");
    // 返回成功
    return true;
}

// 处理 HTTP 请求
bool processHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct HttpResponse *response)
{
    // 清除已有的 headers，防止重复头问题
    response->headerNum = 0;
    bzero(response->statusMsg, sizeof(response->statusMsg));
    response->sendDataFunc = NULL;
    response->sendRangeDataFunc = NULL;
    response->fileFd = -1;
    response->fileOffset = 0;
    response->fileLength = 0;

    // 处理 WebSocket 握手请求
    if (isWebSocketHandshake(request))
    {
        // 处理 WebSocket 握手请求
        // 返回一个合法的 WebSocket 握手响应
        bool flag = handleWebSocketHandshake(request, response);
        if (!flag)
        {
            response->statusCode = BadRequest;
            snprintf(response->statusMsg, sizeof(response->statusMsg), "%s", "Bad Request");
            return false;
        }
        // 处理 WebSocket 握手请求成功
        // 标记连接为 WebSocket 连接
        conn->isWebSocket = true;
        return true;
    }
    if (strcasecmp(request->method, "get") != 0)
    {
        // 说明不是 GET 请求
        // 处理非 GET 请求
        return false;
    }
    // 处理 URL 解码
    // 先将 URL 中的 %XX 解码为对应的字符，然后将 + 替换为空格
    decodeMsg(request->url, request->url);
    // 处理完毕后，URL 应该是一个合法的路径
    // 现在我们需要查找静态资源的路径/文件名
    char *file = NULL;
    // 修改路径，确保使用相对路径
    if (strcmp(request->url, "/") == 0)
    {
        file = "./";
    }
    else
    {
        // 处理成一个合法的路径，确保以 "/" 开头
        file = request->url + 1;
    }
    // 获取文件信息
    // 需要包含头文件 <sys/stat.h>
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        // 获取文件信息失败，说明文件不存在
        // 需要返回一个 404 响应
        snprintf(response->fileName, sizeof(response->fileName), "%s", "404.html");
        response->statusCode = NotFound;
        snprintf(response->statusMsg, sizeof(response->statusMsg), "%s", "Not Found");
        // 添加响应头
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        // 明确告知客户端自己要断开连接
        httpResponseAddHeader(response, "Connection", "close");
        response->sendDataFunc = sendFile;
        return 0;
    }

    snprintf(response->fileName, sizeof(response->fileName), "%s", file);
    response->statusCode = OK;
    snprintf(response->statusMsg, sizeof(response->statusMsg), "%s", "OK");
    // �ж��ļ�����
    if (S_ISDIR(st.st_mode))
    {
        // 说明这是一个目录
        // 这样可以将当前目录下的文件列表返回给客户端
        // 添加响应头
        // 这部分后续需要考虑改用 chunk 分块发送，不然客户端会因为不知道请求体长度而循环等待
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        httpResponseAddHeader(response, "Connection", "close");
        response->sendDataFunc = sendDir;
    }
    else
    {
        // 是一个正常的 GET 请求，我们引入解析 Range 请求的逻辑代码
        const char *rangeHeader = httpRequestGetHeader(request, "Range");
        if (rangeHeader != NULL)
        {
            // 解析 Range 头，格式一般为 "bytes=start-end"
            int start = 0, end = 0;
            if (sscanf(rangeHeader, "bytes=%d-%d", &start, &end) >= 1)
            {
                // 解析成功，我们可以进一步判断
                if (end == 0 || end >= st.st_size)
                {
                    // 如果 end 没有指定，或者请求的范围超过了文件大小，那么我们强制指定其为文件大小 -1
                    end = st.st_size - 1;
                }

                // 计算请求内容的长度
                int contentLength = end - start + 1;

                // 拼响应数据
                response->statusCode = 206; // Partial Content
                snprintf(response->statusMsg, sizeof(response->statusMsg), "%s", "Partial Content");

                // 设置 Content-Range 头
                char contentRange[128];
                sprintf(contentRange, "bytes %d-%d/%ld", start, end, st.st_size);
                httpResponseAddHeader(response, "Content-Range", contentRange);

                // 设置 Accept-Ranges 头
                httpResponseAddHeader(response, "Accept-Ranges", "bytes");

                // 设置 Content-Length 头
                char contentLengthStr[32];
                sprintf(contentLengthStr, "%d", contentLength);
                httpResponseAddHeader(response, "Content-Length", contentLengthStr);
                // 显式告知客户端 keep-alive
                httpResponseAddHeader(response, "Connection", "keep-alive");
                // 明确指定连接超时时间
                httpResponseAddHeader(response, "Keep-Alive", "timeout=5, max=100");

                // 设置 Content-Type 头（Range 响应也需要 Content-Type，浏览器用于识别媒体类型）
                httpResponseAddHeader(response, "Content-Type", getFileType(file));

                // 打开写事件监听
                writeEventEnable(conn->channel, true);
                eventLoopModify(conn->evLoop, conn->channel);
                response->fileOffset = start;
                response->fileLength = contentLength;
                response->fileFd = open(file, O_RDONLY);
                // printf("send partial: Opening a new file descriptor %d!\n", response->fileFd);
                response->isRangeRequest = true;

                response->sendRangeDataFunc = sendRangeRequestData;

                return true;
            }
        }
        // 添加必要的头
        char tmp[12] = {0};
        sprintf(tmp, "%ld", st.st_size);

        httpResponseAddHeader(response, "Content-type", getFileType(file));
        // printf("Got contenttype: %s\n", getFileType(file));
        httpResponseAddHeader(response, "Content-length", tmp);
        // 显式告知客户端 keep-alive
        httpResponseAddHeader(response, "Connection", "keep-alive");
        // 明确指定连接超时时间
        httpResponseAddHeader(response, "Keep-Alive", "timeout=5, max=100");
        // 打开写事件监听
        writeEventEnable(conn->channel, true);
        eventLoopModify(conn->evLoop, conn->channel);
        response->fileOffset = 0;
        response->fileLength = st.st_size;
        response->fileFd = open(file, O_RDONLY);
        // printf("send complete: Opening a new file descriptor %d!\n", response->fileFd);

        // 标记这是一个完整的文件请求，而不是 Range Request
        // 不过我们也复用 sendRangeRequestData 来发送静态文件数据
        response->isRangeRequest = false;
        response->sendRangeDataFunc = sendRangeRequestData;
    }

    return true;
}
