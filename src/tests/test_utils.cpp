// CELICA_HOOK 纯逻辑工具函数单元测试
// 使用 ctest 运行（CTestTestfile），或直接运行本可执行文件
#include "utils.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond, name) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s\n", name); \
            ++g_failures; \
        } else { \
            std::printf("ok: %s\n", name); \
        } \
    } while (0)

int main() {
    // ---- normalizePath ----
    CHECK(Utils::normalizePath(L"a/b/c") == L"a\\b\\c", "normalizePath: 分隔符统一");
    CHECK(Utils::normalizePath(L"a\\..\\b") == L"b", "normalizePath: .. 解析");
    CHECK(Utils::normalizePath(L"a\\b\\..\\..") == L"", "normalizePath: .. 全部弹出");
    CHECK(Utils::normalizePath(L"..\\..\\x") == L"x", "normalizePath: .. 不越过根");
    CHECK(Utils::normalizePath(L".\\a\\.\\b") == L"a\\b", "normalizePath: . 段忽略");
    CHECK(Utils::normalizePath(L"a\\\\b//c") == L"a\\b\\c", "normalizePath: 连续分隔符折叠");
    CHECK(Utils::normalizePath(L"C:\\a\\..\\b") == L"C:\\b", "normalizePath: 绝对路径保留盘符");
    CHECK(Utils::normalizePath(L"v1..2\\file.txt") == L"v1..2\\file.txt", "normalizePath: 合法双点文件名不被破坏");
    CHECK(Utils::normalizePath(L"") == L"", "normalizePath: 空串");

    // ---- splitCommaList ----
    {
        auto list = Utils::splitCommaList(L"a, b ,c");
        CHECK(list.size() == 3 && list[0] == L"a" && list[1] == L"b" && list[2] == L"c",
              "splitCommaList: 去空白");
    }
    CHECK(Utils::splitCommaList(L"  , , ").empty(), "splitCommaList: 空条目忽略");
    CHECK(Utils::splitCommaList(L"").empty(), "splitCommaList: 空串");

    // ---- hexStringToInt ----
    CHECK(Utils::hexStringToInt(L"0x80") == 0x80, "hexStringToInt: 0x前缀");
    CHECK(Utils::hexStringToInt(L"80") == 0x80, "hexStringToInt: 无前缀");
    CHECK(Utils::hexStringToInt(L"zz") == 0, "hexStringToInt: 非法输入返回0");
    CHECK(Utils::hexStringToInt(L"") == 0, "hexStringToInt: 空串返回0");

    // ---- combinePaths ----
    CHECK(Utils::combinePaths(L"a", L"b") == L"a\\b", "combinePaths: 基本");
    CHECK(Utils::combinePaths(L"a\\", L"b") == L"a\\b", "combinePaths: 左侧带分隔符");
    CHECK(Utils::combinePaths(L"a", L"\\b") == L"a\\b", "combinePaths: 右侧带分隔符");
    CHECK(Utils::combinePaths(L"", L"b") == L"b", "combinePaths: 左侧为空");
    CHECK(Utils::combinePaths(L"a", L"") == L"a", "combinePaths: 右侧为空");

    // ---- stripLongPathPrefix ----
    CHECK(Utils::stripLongPathPrefix(L"\\\\?\\C:\\foo") == L"C:\\foo", "stripLongPathPrefix: \\\\?\\ 前缀");
    CHECK(Utils::stripLongPathPrefix(L"C:\\foo") == L"C:\\foo", "stripLongPathPrefix: 无前缀原样");

    // ---- startsWithIgnoreCase ----
    CHECK(Utils::startsWithIgnoreCase(L"Hello\\World", L"hello"), "startsWithIgnoreCase: 命中");
    CHECK(!Utils::startsWithIgnoreCase(L"Hello", L"hello2"), "startsWithIgnoreCase: 前缀更长");

    // ---- isAbsolutePath ----
    CHECK(Utils::isAbsolutePath(L"C:\\game\\x.exe"), "isAbsolutePath: 盘符路径");
    CHECK(Utils::isAbsolutePath(L"\\\\server\\share\\x"), "isAbsolutePath: UNC路径");
    CHECK(!Utils::isAbsolutePath(L"game\\x.exe"), "isAbsolutePath: 相对路径");
    CHECK(!Utils::isAbsolutePath(L""), "isAbsolutePath: 空串");

    // ---- getFileName / getDirectory ----
    CHECK(Utils::getFileName(L"a\\b\\c.txt") == L"c.txt", "getFileName");
    CHECK(Utils::getDirectory(L"a\\b\\c.txt") == L"a\\b", "getDirectory");
    CHECK(Utils::getFileName(L"c.txt") == L"c.txt", "getFileName: 无目录");

    // ---- toLower ----
    CHECK(Utils::toLower(L"AbC") == L"abc", "toLower");

    // ---- isValidExtension ----
    CHECK(Utils::isValidExtension(L"a.txt", L".txt,.bin"), "isValidExtension: 命中");
    CHECK(!Utils::isValidExtension(L"a.jpg", L".txt,.bin"), "isValidExtension: 未命中");
    CHECK(Utils::isValidExtension(L"a.jpg", L""), "isValidExtension: 空列表=全部允许");

    if (g_failures == 0) {
        std::printf("全部测试通过\n");
        return 0;
    }
    std::printf("%d 个测试失败\n", g_failures);
    return 1;
}
