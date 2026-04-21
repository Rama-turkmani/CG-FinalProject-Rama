// ============================================================================
//  Final Project - Computer Graphics (Modern OpenGL 3.3 Core)
//  3D Maze Collector - Student : Rama
//  Libraries : GLFW, GLEW, GLM, stb_image
// ============================================================================
//  [AR] مشروع مادة بيانيات الحاسوب النهائي
//  ------------------------------------------------------------
//  الفكرة :
//    - لعبة منظور الشخص الأول داخل متاهة ثلاثية الأبعاد (شبكة 10x10).
//    - اللاعب يجمع 5 عملات (بخامة awesomeface) قبل انتهاء مؤقّت 90 ثانية.
//    - الجدران والأرضية بخامة container.jpg.
//    - تأثيرات بصرية : إضاءة موجّهة (Directional Diffuse) + ضباب أُسّي.
//    - تصادم : دائرة اللاعب ضد AABB لكل مكعّب جدار، بفصل المحاور للانزلاق.
//    - HUD : 5 مربعات لحالة العملات + شريط مؤقّت + طبقة فوز/خسارة.
//
//  التحكّم (الزوج الخامس من المعايير) :
//    W / S / A / D : تقدّم / رجوع / انحراف يسار / انحراف يمين
//    Mouse         : النظر (yaw / pitch)
//    R             : إعادة اللعبة
//    ESC           : خروج
//
//  تغطية المعايير (14 علامة) :
//    الزوج 1  : GLFW + GLEW + حلقة اللعبة وفصل المنطق عن الرسم.
//    الزوج 2  : VAO + VBO + EBO (الأرضية) ومصفوفة MVP في world.vert.
//    الزوج 3  : كاميرا منظوريّة + yaw/pitch مع clamp على pitch.
//    الزوج 4  : خامات (Mipmaps + Trilinear) + إضاءة + ضباب.
//    الزوج 5  : إدخال WASD نسبي للـ yaw + فانوس مرئي كشخصية 3D.
//    الزوج 6  : تصادم AABB + متاهة ببيانات نصّية.
//    الزوج 7  : منطق لعبة (فوز/خسارة/مؤقّت) + HUD كامل.
// ============================================================================

// ----------------------------------------------------------------------------
//  [AR] الترويسات القياسية في لغة C++
//  iostream  : للطباعة على الكونسول (cerr / cout).
//  fstream   : لقراءة ملفّات الشيدر من القرص.
//  sstream   : لجمع محتوى الملف في نص واحد (stringstream).
//  string/vector/array : حاويات STL نستخدمها للقوائم الديناميكيّة.
//  cmath     : دوال sin / cos / sqrt.
// ----------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cmath>

// ----------------------------------------------------------------------------
//  [AR] مكتبات OpenGL
//  GLEW_STATIC : نربط GLEW ستاتيكياً (ملف glew32s.lib) حتى لا نحتاج DLL.
//  glew.h      : محمّل دوال OpenGL الحديثة وقت التشغيل (glGenBuffers..).
//  glfw3.h     : إنشاء النافذة وسياق OpenGL واستقبال الكيبورد/الماوس.
// ----------------------------------------------------------------------------
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// ----------------------------------------------------------------------------
//  [AR] مكتبة GLM (رياضيات متجهات ومصفوفات مطابقة لـ GLSL)
//  glm.hpp                  : vec2/vec3/vec4 و mat3/mat4.
//  gtc/matrix_transform.hpp : دوال translate / rotate / scale / perspective / lookAt.
//  gtc/type_ptr.hpp         : value_ptr() لتحويل المصفوفة إلى مؤشّر float*
//                            كي نرسلها لـ OpenGL عبر glUniformMatrix4fv.
// ----------------------------------------------------------------------------
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ----------------------------------------------------------------------------
//  [AR] مكتبة stb_image لفكّ تشفير الصور (JPG / PNG)
//  لأنها مكتبة Header-only يجب تعريف STB_IMAGE_IMPLEMENTATION في ملف واحد فقط
//  ليتمّ توليد التنفيذ الفعلي للدوال.
// ----------------------------------------------------------------------------
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================================
//  [AR] ثوابت اللعبة والنافذة (Tunables)
//  ----------------------------------------------------------------------------
//  كل الأرقام "السحريّة" محصورة هنا بحيث يسهل تغيير أيّ سلوك بسطر واحد
//  دون البحث داخل منطق اللعبة. هذا أسلوب قياسي في تصميم المحرّكات.
// ============================================================================
static unsigned int SCR_W = 1024;          // عرض النافذة الابتدائي بالبكسل
static unsigned int SCR_H = 720;           // ارتفاع النافذة الابتدائي بالبكسل
static const float  CELL  = 2.0f;          // طول ضلع خلية المتاهة بالوحدات العالميّة
static const float  PLAYER_R = 0.35f;      // نصف قطر دائرة تصادم اللاعب
static const float  PLAYER_EYE_Y = 0.9f;   // ارتفاع عين اللاعب عن الأرضيّة (Y)
static const float  PLAYER_SPEED = 3.2f;   // سرعة المشي (وحدة عالميّة/ثانية)
static const float  MOUSE_SENS = 0.11f;    // حساسيّة الماوس (درجة/بكسل)
static const float  GAME_TIME = 90.0f;     // زمن اللعبة الكلي بالثواني

// ----------------------------------------------------------------------------
//  [AR] شبكة المتاهة 10x10 كنصّ
//  ----------------------------------------------------------------------------
//  هذا أسلوب Data-Driven Design :
//    نصف تخطيط المتاهة كبيانات (نصّ) لا ككود. لتغيير المتاهة يكفي تعديل
//    الأحرف أدناه. تقوم buildLevel() بقراءة هذه الشبكة وتحويلها إلى
//    قائمة مكعّبات جدران + قائمة عملات + موقع انطلاق اللاعب.
//
//  الرموز :
//    'W' = جدار (مكعّب كامل)
//    '.' = أرضيّة فارغة
//    'C' = أرضيّة + عملة قابلة للجمع
//    'S' = نقطة انطلاق اللاعب
//
//  يجب أن تكون كل الحافّات جدراناً لنضمن ساحة مغلقة.
// ----------------------------------------------------------------------------
static const char* MAZE[10] = {
    "WWWWWWWWWW",
    "WS.....C.W",
    "W.WWW.WW.W",
    "W.W.C..W.W",
    "W.W.WW.W.W",
    "W.W..W.W.W",
    "W.WWWW.W.W",
    "W.C..W.C.W",
    "W.WW.W.W.W",
    "WWWWWWWCWW",
};
static const int MAZE_W = 10;              // عدد أعمدة المتاهة
static const int MAZE_H = 10;              // عدد صفوف المتاهة

// ============================================================================
//  [AR] بنية اللاعب / الكاميرا (Player/Camera State)
//  ----------------------------------------------------------------------------
//  اللعبة منظور الشخص الأول، لذلك الكاميرا هي اللاعب نفسه :
//    pos   = موقع العين في العالم (x, y=PLAYER_EYE_Y, z).
//    yaw   = زاوية الدوران حول المحور العمودي Y بالدرجات.
//            0° تنظر نحو +X، و 90° نحو +Z.
//    pitch = زاوية الميل لأعلى/أسفل. مقيّدة بـ ±85° لتجنّب انقلاب المحاور
//            (Gimbal Flip) عند النظر عمودياً.
// ============================================================================
struct Player {
    glm::vec3 pos;
    float yaw;   // بالدرجات : 0 = +X ، 90 = +Z
    float pitch; // بالدرجات : مُقيَّدة ±85° داخل mouseCallback
};

// ----------------------------------------------------------------------------
//  [AR] متغيّرات عامّة (Globals)
//  السبب : Callbacks الخاصة بـ GLFW لا تستقبل أي this، لذا لا يمكن لها الوصول
//  لحقول كائن ما. الحلّ المعتاد هو متغيّرات ملفيّة static يصل إليها الجميع.
// ----------------------------------------------------------------------------
static Player     g_player;                      // حالة اللاعب الحاليّة
static bool       g_firstMouse = true;           // علم لتفادي قفزة أوّل حدث ماوس
static double     g_lastMouseX = 0.0, g_lastMouseY = 0.0;  // آخر موقع للماوس
static float      g_deltaTime = 0.0f;            // زمن الإطار بالثواني
static float      g_lastFrame = 0.0f;            // الزمن عند بداية الإطار السابق

// ============================================================================
//  [AR] حالة اللعبة (Game State)
//  ----------------------------------------------------------------------------
//  اللعبة آلة حالات بسيطة (Finite State Machine) بثلاث حالات فقط :
//    Playing : اللعب جار، الإدخال مُفعَّل، المؤقّت ينزل.
//    Won     : جمع اللاعب كل العملات    → طبقة خضراء.
//    Lost    : انتهى المؤقّت قبل الجمع   → طبقة حمراء.
//  الانتقال يحدث داخل updateGame() حسب قيمتي g_coinsLeft و g_timeLeft.
// ============================================================================
enum class GameState { Playing, Won, Lost };

// بنية العملة : موقعها، هل ما زالت على الأرض، وزاوية دورانها الحاليّة.
struct Coin {
    glm::vec3 pos;     // موقع العملة في العالم
    bool      alive;   // true = لم تُجمع بعد
    float     spin;    // زاوية الدوران بالراديان (لإضفاء حياة بصريّة)
};

// ----------------------------------------------------------------------------
//  [AR] متغيّرات عامّة لحالة اللعبة الحيّة
//  تُكتب من processInput / updateGame / buildLevel، وتُقرأ كل إطار للرسم.
// ----------------------------------------------------------------------------
static GameState              g_state = GameState::Playing;  // الحالة الحاليّة
static std::vector<Coin>      g_coins;                       // قائمة العملات
static std::vector<glm::vec3> g_walls;      // مركز كل مكعّب جدار (y=1)
static float                  g_timeLeft = GAME_TIME;        // الثواني المتبقيّة
static int                    g_coinsLeft = 0;               // عدد العملات الحيّة
static bool                   g_isMoving = false;            // للتمايل أثناء المشي
static float                  g_walkPhase = 0.0f;            // طور التمايل (راديان)

// ============================================================================
//  [AR] مساعدات الملفّات والشيدرات
// ============================================================================

// ----------------------------------------------------------------------------
//  [AR] findAsset : البحث عن ملفّ بمسار نسبي
//  ----------------------------------------------------------------------------
//  المشكلة : حسب طريقة تشغيل الـ .exe تختلف مجلد العمل (CWD) :
//    - من Visual Studio        → CWD = جذر الحل، فالملفات في "Shaders/.." مباشرة.
//    - من x64\Debug\app.exe    → CWD = x64/Debug، فالملفات في "../../Shaders/.."
//    - من مجلد project1      → CWD = project1، فالملفات في "../Shaders/.."
//  الحلّ : نجرّب الأربع بادئات المحتملة ونعيد أول مسار موجود.
//  هذا يشبع متطلّب المعايير : "المشروع يعمل على أي جهاز بمسارات نسبية".
// ----------------------------------------------------------------------------
static std::string findAsset(const std::string& rel)
{
    // جرّب مجلد العمل ثمّ المجلدات الأعلى حتى 3 مستويات.
    const char* prefixes[] = { "", "../", "../../", "../../../" };
    for (const char* p : prefixes) {
        std::string full = std::string(p) + rel;
        // نفتح الملفّ بوضع binary لنتفادى أي تحويل لنهايات الأسطر.
        std::ifstream f(full.c_str(), std::ios::binary);
        if (f.good()) return full;      // وجدناه → أعد المسار كاملاً
    }
    // لم نجده → نظهر تحذيراً ونعيد المسار النسبي كما هو لتظهر رسالة الخطأ اللاحقة.
    std::cerr << "[WARN] Could not locate asset: " << rel << "\n";
    return rel;
}

// ----------------------------------------------------------------------------
//  [AR] readTextFile : قراءة ملفّ نصّي بالكامل
//  تُستخدم لقراءة ملفّات GLSL (.vert / .frag).
//  تستخدم stringstream لجمع المحتوى في string واحد دفعة واحدة.
// ----------------------------------------------------------------------------
static std::string readTextFile(const std::string& path)
{
    std::ifstream f(path.c_str());
    if (!f.good()) {
        // الملفّ غير موجود → لن يتصرّف الشيدر وسيفشل التصريف، لكن نطبع خطأ واضحاً أولاً.
        std::cerr << "[ERR ] Cannot open file: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();     // يقرأ المحتوى دفعة واحدة إلى الـ stringstream.
    return ss.str();     // تحويل الـ stringstream إلى std::string.
}

// ----------------------------------------------------------------------------
//  [AR] compileShader : تصريف نصّ GLSL إلى شيدر واحد (Vertex أو Fragment)
//  ----------------------------------------------------------------------------
//  الخطوات :
//    1) glCreateShader(type)    : ينشئ كائن شيدر فارغ من النوع المطلوب.
//    2) glShaderSource           : ربط النص المصدري بالكائن.
//    3) glCompileShader          : تصريف فعلي داخل التعريف (الـ GPU Driver).
//    4) glGetShaderiv(..COMPILE_STATUS) + glGetShaderInfoLog : لاختبار النجاح.
//  الوسم tag لا يستخدمه OpenGL بل نطبعه في رسالة الخطأ ليعرف القارئ الملف الذي فشل.
// ----------------------------------------------------------------------------
static unsigned int compileShader(GLenum type, const std::string& src, const std::string& tag)
{
    unsigned int s = glCreateShader(type);                   // إنشاء الكائن
    const char* c = src.c_str();                             // GLSL يحتاج const char*
    glShaderSource(s, 1, &c, nullptr);                       // ربط المصدر
    glCompileShader(s);                                      // التصريف الفعلي
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);                // هل نجح التصريف؟
    if (!ok) {
        // طباعة سجلّ الخطأ التفصيلي لنعرف أين الخطأ في شيفرة GLSL.
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "[ERR ] Shader compile (" << tag << "):\n" << log << "\n";
    }
    return s;
}

// ----------------------------------------------------------------------------
//  [AR] makeProgram : بناء برنامج شيدر كامل (Vertex + Fragment مرتبطين)
//  ----------------------------------------------------------------------------
//  برنامج الشيدر (GLProgram) هو وحدة قابلة للاستخدام عبر glUseProgram.
//  الخطوات :
//    1) قراءة ملفّي GLSL من القرص.
//    2) تصريف كل منهما عبر compileShader.
//    3) glCreateProgram ثمّ glAttachShader لكل شيدر.
//    4) glLinkProgram : ربط خرج الـ Vertex Shader بدخل الـ Fragment Shader.
//    5) التحقّق من نجاح الربط عبر GL_LINK_STATUS.
//    6) glDeleteShader لـ vs و fs : البرنامج احتفظ بنسخته المرتبطة، وهذا الحذف
//       يحرّر بيانات التصريف الوسيطة فقط. (سؤال شائع في المناقشة!)
// ----------------------------------------------------------------------------
static unsigned int makeProgram(const std::string& vsRel, const std::string& fsRel)
{
    // (1) قراءة ملفّيّ الشيدر من القرص :
    std::string vsSrc = readTextFile(findAsset(vsRel));
    std::string fsSrc = readTextFile(findAsset(fsRel));
    // (2) تصريفهما :
    unsigned int vs = compileShader(GL_VERTEX_SHADER,   vsSrc, vsRel);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fsSrc, fsRel);
    // (3) إنشاء برنامج وإرفاق الشيدرين :
    unsigned int p  = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    // (4) ربط البرنامج (يتأكد أن خرج Vertex Shader يناسب دخل Fragment Shader) :
    glLinkProgram(p);
    // (5) التحقّق من نجاح الربط :
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "[ERR ] Program link (" << vsRel << " + " << fsRel << "):\n" << log << "\n";
    }
    // (6) لم نعد بحاجة للشيدرات بعد الربط → تحريرها :
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

// ============================================================================
//  [AR] loadTexture : تحميل صورة من القرص إلى خامة OpenGL
//  ----------------------------------------------------------------------------
//  الخطوات :
//    1) findAsset     : على مسار نسبي أياً كان مجلد العمل.
//    2) قلب الصورة عمودياً : ملفّات الصور تبدأ من الأعلى-اليسار، بينما إحداثيات
//       الخامة في OpenGL (UV) تبدأ من الأسفل-اليسار. إن لم نقلبها ستظهر مقلوبة.
//    3) stbi_load     : فكّ تشفير الملف إلى مصفوفة بايتات خام + إعادة العرض/الارتفاع/عدد القنوات.
//    4) glGenTextures + glBindTexture : حجز رقم خامة وجعلها الهدف الحالي.
//    5) glTexParameteri :
//         - WRAP_S/T = REPEAT : تكرار الخامة (مناسب للأرضيّة المبلّطة).
//         - MIN_FILTER = LINEAR_MIPMAP_LINEAR : تصفية Trilinear للسطوح البعيدة (نعومة عالية).
//         - MAG_FILTER = LINEAR : Bilinear عند التكبير (الـ Mipmaps لا تنطبق عند التكبير).
//    6) glTexImage2D    : رفع البايتات إلى الـ GPU.
//    7) glGenerateMipmap : يبني سلسلة التصغيرات التي يحتاجها الـ Trilinear.
//    8) stbi_image_free : تحرير ذاكرة CPU (البيانات الآن على الـ GPU).
// ============================================================================
static unsigned int loadTexture(const std::string& relPath)
{
    std::string full = findAsset(relPath);

    // (2) قلب الصورة عمودياً لتتوافق مع اتجاه UV في OpenGL.
    stbi_set_flip_vertically_on_load(true);

    // (3) فكّ التشفير : w = عرض، h = ارتفاع، n = عدد القنوات (3=RGB، 4=RGBA).
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(full.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "[ERR ] Failed to load texture: " << full << "\n";
        return 0;
    }

    // (4) حجز رقم خامة وربطها كهدف حالي لـ GL_TEXTURE_2D.
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // (5) ضبط طرق التكرار والتصفية.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);             // تكرار على المحور U
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);             // تكرار على المحور V
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Trilinear عند التصغير
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);         // Bilinear عند التكبير

    // (6) تحديد صيغة الـ GPU حسب عدد القنوات المجلوبة.
    GLenum fmt = (n == 4) ? GL_RGBA : (n == 3) ? GL_RGB : GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);

    // (7) بناء Mipmaps وحده : الـ GPU يولّد نسخاً مصغّرة من الخامة.
    glGenerateMipmap(GL_TEXTURE_2D);

    // (8) تحرير ذاكرة الـ CPU — البيانات منسوخة على الـ GPU الآن.
    stbi_image_free(data);
    return tex;
}

// ============================================================================
//  [AR] بناة الأشكال (Mesh Builders)
//  ----------------------------------------------------------------------------
//  تخطيط كل رأس في أشكال العالم (Cube/Floor/Quad) :
//     [pos : 3 floats] [uv : 2 floats] [normal : 3 floats] = 8 فاصلة لكل رأس.
//  هذا يناسب تماماً تخطيط الأماكن في world.vert :
//     layout(location=0) vec3 aPos     → offset 0
//     layout(location=1) vec2 aUV      → offset 3*float
//     layout(location=2) vec3 aNormal  → offset 5*float
//  الخطوة stride = 8 * sizeof(float).
// ============================================================================

// ----------------------------------------------------------------------------
//  [AR] makeCubeVAO : مكعّب وحدوي (للجدران والفانوس)
//  ----------------------------------------------------------------------------
//  المكعّب له 6 أوجه. كل وجه يتكوّن من مثلثين (6 رؤوس) → 36 رأساً إجمالاً.
//  لماذا لا نشترك في الرؤوس باستخدام EBO؟
//    كل وجه يحتاج نورمال مختلف وإحداثيات UV مختلفة، فلا توجد فائدة من المشاركة.
//    لذلك نستخدم VAO + VBO فقط و glDrawArrays دون EBO.
//  محددات الوجوه : -Z, +Z, -X, +X, -Y, +Y (ستة أوجه).
//  يعيد رقم الـ VAO، ويكتب رقم الـ VBO في المعامل outVBO لنحرّره لاحقاً.
// ----------------------------------------------------------------------------
static unsigned int makeCubeVAO(unsigned int& outVBO)
{
    // مكعّب وحدوي مركزه الأصل، طول ضلعه 1 (يضرب في مصفوفة النموذج Model للتكبير).
    float v[] = {
        // pos               uv         normal
        // -Z
        -0.5f,-0.5f,-0.5f,  0,0,   0, 0,-1,
         0.5f,-0.5f,-0.5f,  1,0,   0, 0,-1,
         0.5f, 0.5f,-0.5f,  1,1,   0, 0,-1,
         0.5f, 0.5f,-0.5f,  1,1,   0, 0,-1,
        -0.5f, 0.5f,-0.5f,  0,1,   0, 0,-1,
        -0.5f,-0.5f,-0.5f,  0,0,   0, 0,-1,
        // +Z
        -0.5f,-0.5f, 0.5f,  0,0,   0, 0, 1,
         0.5f,-0.5f, 0.5f,  1,0,   0, 0, 1,
         0.5f, 0.5f, 0.5f,  1,1,   0, 0, 1,
         0.5f, 0.5f, 0.5f,  1,1,   0, 0, 1,
        -0.5f, 0.5f, 0.5f,  0,1,   0, 0, 1,
        -0.5f,-0.5f, 0.5f,  0,0,   0, 0, 1,
        // -X
        -0.5f, 0.5f, 0.5f,  1,1,  -1, 0, 0,
        -0.5f, 0.5f,-0.5f,  0,1,  -1, 0, 0,
        -0.5f,-0.5f,-0.5f,  0,0,  -1, 0, 0,
        -0.5f,-0.5f,-0.5f,  0,0,  -1, 0, 0,
        -0.5f,-0.5f, 0.5f,  1,0,  -1, 0, 0,
        -0.5f, 0.5f, 0.5f,  1,1,  -1, 0, 0,
        // +X
         0.5f, 0.5f, 0.5f,  1,1,   1, 0, 0,
         0.5f, 0.5f,-0.5f,  0,1,   1, 0, 0,
         0.5f,-0.5f,-0.5f,  0,0,   1, 0, 0,
         0.5f,-0.5f,-0.5f,  0,0,   1, 0, 0,
         0.5f,-0.5f, 0.5f,  1,0,   1, 0, 0,
         0.5f, 0.5f, 0.5f,  1,1,   1, 0, 0,
        // -Y
        -0.5f,-0.5f,-0.5f,  0,1,   0,-1, 0,
         0.5f,-0.5f,-0.5f,  1,1,   0,-1, 0,
         0.5f,-0.5f, 0.5f,  1,0,   0,-1, 0,
         0.5f,-0.5f, 0.5f,  1,0,   0,-1, 0,
        -0.5f,-0.5f, 0.5f,  0,0,   0,-1, 0,
        -0.5f,-0.5f,-0.5f,  0,1,   0,-1, 0,
        // +Y
        -0.5f, 0.5f,-0.5f,  0,1,   0, 1, 0,
         0.5f, 0.5f,-0.5f,  1,1,   0, 1, 0,
         0.5f, 0.5f, 0.5f,  1,0,   0, 1, 0,
         0.5f, 0.5f, 0.5f,  1,0,   0, 1, 0,
        -0.5f, 0.5f, 0.5f,  0,0,   0, 1, 0,
        -0.5f, 0.5f,-0.5f,  0,1,   0, 1, 0,
    };
    // --- (أ) حجز كائني VAO و VBO ---------------------------------------------
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);       // VAO : يحفظ تخطيط الصفات (stride, offsets).
    glGenBuffers(1, &vbo);            // VBO : يحفظ البايتات الخام للرؤوس على الـ GPU.
    glBindVertexArray(vao);           // أي إعدادات تالية ستُسجل ضمن هذا الـ VAO.

    // --- (ب) رفع بيانات الرؤوس إلى الـ GPU -----------------------------------
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // GL_STATIC_DRAW : البيانات لن تتغيّر بعد الرفع (تلميح للتعريف ليضعها في ذاكرة سريعة).
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    // --- (ج) وصف تخطيط الرأس لـ OpenGL : 3 صفات بتوازي stride=8 float --------
    const GLsizei stride = 8 * sizeof(float);
    //   location=0 : vec3 pos    → offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    //   location=1 : vec2 uv     → offset 3*float
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    //   location=2 : vec3 normal → offset 5*float
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);

    outVBO = vbo;     // نعيد رقم الـ VBO للمتصل ليحذفه في النهاية.
    return vao;
}

// ----------------------------------------------------------------------------
//  [AR] makeFloorVAO : الأرضية باستخدام VAO + VBO + EBO (حرج للمعايير!)
//  ----------------------------------------------------------------------------
//  الإلزام في الزوج الثاني : "بين للحارس ثلاثي التصنيف VAO / VBO / EBO".
//  الأرضية مربّع مستوٍ واحد يحتوي 4 أركان فقط. كل مثلّث يحتاج 3 رؤوس، والأرضية
//  مثلّثان. لو استخدمنا glDrawArrays لكان علينا تكرار رأسين من الربعة (6 رؤوس).
//  بالـ EBO نكتفي بـ 4 رؤوس فقط + مصفوفة فهارس {0,1,2, 2,3,0}، ونرسم بـ glDrawElements.
//  التوفير هنا رمزي (برؤوس فقط)، لكن في الأشكال الكبيرة التوفير جوهري جداً.
//  uvTile : عدد مرات تكرار خامة الأرض أفقياً وعمودياً (تُحدّد عند الاستدعاء).
// ----------------------------------------------------------------------------
static unsigned int makeFloorVAO(unsigned int& outVBO, unsigned int& outEBO, float half, float uvTile)
{
    float y = 0.0f;
    // الرؤوس الأربعة فقط (ليس 12!) : UV يتدرّج من 0 إلى uvTile ليكرّر الخامة.
    float v[] = {
        // pos                  uv                 normal
        -half, y, -half,        0.0f,   0.0f,      0,1,0,   // 0 : الركن الخلفي-اليساري
         half, y, -half,        uvTile, 0.0f,      0,1,0,   // 1 : الركن الخلفي-اليميني
         half, y,  half,        uvTile, uvTile,    0,1,0,   // 2 : الركن الأمامي-اليميني
        -half, y,  half,        0.0f,   uvTile,    0,1,0,   // 3 : الركن الأمامي-اليساري
    };
    // مصفوفة الفهارس (EBO) : 6 أرقام تشير إلى الرؤوس أعلاه.
    unsigned int i[] = {
        0, 1, 2,   // المثلّث الأول : الخلفي-اليساري → الخلفي-اليميني → الأمامي-اليميني
        2, 3, 0,   // المثلّث الثاني : يُكمل المربّع (نلاحظ إعادة استخدام الرأسين 2 و 0)
    };

    unsigned int vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);                                    // هذا هو الـ EBO!
    glBindVertexArray(vao);

    // رفع بايتات الرؤوس :
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    // رفع بايتات الفهارس إلى الـ GPU :
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i, GL_STATIC_DRAW);

    // وصف تخطيط الرأس (نفس تخطيط المكعّب) :
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);

    outVBO = vbo;
    outEBO = ebo;
    return vao;
}

// ----------------------------------------------------------------------------
//  [AR] makeQuadVAO : مربّع العملة (ثنائي الوجه)
//  ----------------------------------------------------------------------------
//  العملة مربّع مسطّح في المستوى XY (z=0) بحجم 0.7 وحدة.
//  6 رؤوس = مثلّثان (نستخدم glDrawArrays لا EBO هنا).
//  النورمال +Z. في مرحلة الرسم نُوقف culling ليظهر الوجهان معاً.
// ----------------------------------------------------------------------------
static unsigned int makeQuadVAO(unsigned int& outVBO)
{
    // 6 رؤوس تشكل مربّعاً في مستوى XY. النورمال (0,0,1) → +Z.
    float v[] = {
        // pos              uv         normal (+Z)
        -0.35f,-0.35f, 0,   0,0,       0,0,1,
         0.35f,-0.35f, 0,   1,0,       0,0,1,
         0.35f, 0.35f, 0,   1,1,       0,0,1,
         0.35f, 0.35f, 0,   1,1,       0,0,1,
        -0.35f, 0.35f, 0,   0,1,       0,0,1,
        -0.35f,-0.35f, 0,   0,0,       0,0,1,
    };
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);
    outVBO = vbo;
    return vao;
}

// ----------------------------------------------------------------------------
//  [AR] makeHudQuadVAO : مربّع HUD وحدوي بموقع فقط (0..1)
//  ----------------------------------------------------------------------------
//  يُرسم في إحداثيات الشاشة بعد ضربه بمصفوفة ortho داخل hud.vert.
//  لا يحتاج UV أو Normal — لونه يأتي من uColor (يُرسل كـ Uniform لكل مربّع).
//  المربّع يُقاس ويُراحَل في hud.vert عبر uOffset و uSize.
// ----------------------------------------------------------------------------
static unsigned int makeHudQuadVAO(unsigned int& outVBO)
{
    // 6 رؤوس (مثلثان) في المجال [0..1]×[0..1].
    float v[] = {
        0,0,  1,0,  1,1,
        1,1,  0,1,  0,0,
    };
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    // صفة واحدة فقط : vec2 aPos → location 0.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    outVBO = vbo;
    return vao;
}

// ============================================================================
//  [AR] مساعدات المتاهة
// ============================================================================

// ----------------------------------------------------------------------------
//  [AR] cellToWorld : تحويل إحداثيات خلية المتاهة (cx, cz) إلى إحداثيات عالميّة
//  ----------------------------------------------------------------------------
//  المتاهة مركزها على الأصل (0,0,0). لحساب مركز أي خلية :
//    1) نطرح (MAZE_W / 2) لنقل المرجع من الزاوية إلى المركز.
//    2) نضيف 0.5 لنقف في منتصف الخلية (لا عند حافّتها).
//    3) نضرب في CELL للتحويل إلى الوحدات العالميّة.
// ----------------------------------------------------------------------------
static glm::vec3 cellToWorld(int cx, int cz)
{
    // مركز الخلية في الفضاء العالمي. الشبكة مركزها على الأصل.
    float wx = (cx - MAZE_W * 0.5f + 0.5f) * CELL;
    float wz = (cz - MAZE_H * 0.5f + 0.5f) * CELL;
    return glm::vec3(wx, 0.0f, wz);
}

// ----------------------------------------------------------------------------
//  [AR] cellIsWall : هل الخلية (cx,cz) جدار؟
//  نتعامل مع الإحداثيات خارج الشبكة كأنها جدران (حماية إضافية).
//  ملاحظة : غير مستخدمة حالياً في التصادم (لأننا نفحص ضد مواقع AABB مباشرة).
// ----------------------------------------------------------------------------
static bool cellIsWall(int cx, int cz)
{
    if (cx < 0 || cx >= MAZE_W || cz < 0 || cz >= MAZE_H) return true;
    return MAZE[cz][cx] == 'W';
}

// ----------------------------------------------------------------------------
//  [AR] buildLevel : تحويل شبكة MAZE النصّيّة إلى عالم ثلاثي الأبعاد
//  ----------------------------------------------------------------------------
//  تُستدعى :
//    - عند بدء اللعبة.
//    - عند الضغط على R (إعادة من داخل processInput).
//  ماذا تفعل :
//    1) تفرّغ القوائم الحاليّة للجدران والعملات.
//    2) تمسح المتاهة خلية خلية :
//         - 'W' : تضيف مركز جدار جديد في g_walls.
//         - 'C' : تضيف عملة جديدة (alive=true, spin=0).
//         - 'S' : تضع اللاعب هنا وتصفر زواياه.
//    3) تُصفّر المؤقّت وتضع الحالة على Playing.
//    4) g_firstMouse = true : تفادى قفزة ماوس فجائية بعد الإعادة.
// ----------------------------------------------------------------------------
static void buildLevel()
{
    g_walls.clear();
    g_coins.clear();
    // مسح كل خلية في الشبكة (الحلقتان الخارجيّة والداخليّة).
    for (int z = 0; z < MAZE_H; ++z) {
        for (int x = 0; x < MAZE_W; ++x) {
            char c = MAZE[z][x];
            glm::vec3 center = cellToWorld(x, z);
            if (c == 'W') {
                // جدار : مركز مكعّب على y=1 لأن طول المكعّب 2 وقاعدته تلامس الأرض.
                g_walls.push_back(glm::vec3(center.x, 1.0f, center.z));
            }
            else if (c == 'C') {
                // عملة : مرتفعة قليلاً عن الأرض (y=0.8) لتظهر واضحة.
                g_coins.push_back({ glm::vec3(center.x, 0.8f, center.z), true, 0.0f });
            }
            else if (c == 'S') {
                // موقع انطلاق اللاعب : y = ارتفاع العين (PLAYER_EYE_Y).
                g_player.pos   = glm::vec3(center.x, PLAYER_EYE_Y, center.z);
                g_player.yaw   = 0.0f;
                g_player.pitch = 0.0f;
            }
        }
    }
    // إعداد المؤقّت والحالة (مهمّ عند الإعادة بـ R).
    g_coinsLeft = (int)g_coins.size();
    g_timeLeft  = GAME_TIME;
    g_state     = GameState::Playing;
    g_firstMouse = true;       // لنتفادى قفزة ماوس حادة عند أوّل حركة بعد الإعادة.
}

// ============================================================================
//  [AR] التصادم : دائرة اللاعب ضد صندوق الجدار (AABB vs Circle) في مستوى XZ
//  ----------------------------------------------------------------------------
//  الفكرة :
//    - كل مكعّب جدار له بصمة مربّعة CELL×CELL محاذية للمحاور (AABB).
//    - اللاعب دائرة نصف قطرها PLAYER_R في مستوى XZ.
//    - نستخدم اختبار "أقرب نقطة على الـ AABB" :
//         نقصّ موقع اللاعب داخل الـ AABB بـ clamp، ثمّ نقيس المسافة بين اللاعب
//         وهذه النقطة. إن كانت أصغر من نصف القطر → تصادم.
//    - نستخدم المسافة التربيعيّة (dx²+dz² < r²) لتفادي sqrt المكلفة.
//
//  الانزلاق : يحدث داخل processInput بفصل المحاور (نجرّب X أولاً، ثم Z، وإن
//  اصطدمت إحداهما نلغيه وحده). هذا يسمح للاعب بالانزلاق على الجدار بدل الالتصاق.
// ============================================================================
static bool collides(const glm::vec3& p)
{
    const float half = CELL * 0.5f;   // نصف ضلع الجدار.
    for (const glm::vec3& w : g_walls) {
        // (1) احسب حدود صندوق الجدار (AABB) في X و Z.
        float minX = w.x - half, maxX = w.x + half;
        float minZ = w.z - half, maxZ = w.z + half;
        // (2) حصر موقع اللاعب داخل الصندوق → أقرب نقطة على الـ AABB.
        float cx = glm::clamp(p.x, minX, maxX);
        float cz = glm::clamp(p.z, minZ, maxZ);
        // (3) المسافة بين اللاعب وتلك النقطة (مربّع المسافة).
        float dx = p.x - cx;
        float dz = p.z - cz;
        // (4) إن كانت أصغر من نصف القطر (التربيعي) → تداخل → تصادم.
        if (dx * dx + dz * dz < PLAYER_R * PLAYER_R) return true;
    }
    return false;   // لا يوجد تصادم مع أي جدار.
}

// ============================================================================
//  [AR] الإدخال (Input)
// ============================================================================

// ----------------------------------------------------------------------------
//  [AR] mouseCallback : دالة استدعاء من GLFW عند تحريك الماوس
//  ----------------------------------------------------------------------------
//  تحول حركة الماوس إلى تغيّر في yaw / pitch :
//    dx : فرق الحركة الأفقيّة  → yaw  (الدوران يمين/يسار).
//    dy : فرق الحركة العموديّة → pitch (النظر لأعلى/أسفل).
//  ملاحظة : نعكس dy لأن محور Y في الشاشة ينمو لأسفل لكن نريده ينمو لأعلى.
//  الـ clamp ±85° مهمّ : يمنع انقلاب المحاور (Gimbal Flip) عند النظر عمودياً بالتمام.
// ----------------------------------------------------------------------------
static void mouseCallback(GLFWwindow*, double xpos, double ypos)
{
    // عند أوّل حدث ماوس : نسجل موقعه فقط دون تطبيق فرقاً لتفادي قفزة كبيرة.
    if (g_firstMouse) {
        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_firstMouse = false;
        return;
    }
    // حساب الفروق وضربها في حساسيّة الماوس.
    float dx = (float)(xpos - g_lastMouseX) * MOUSE_SENS;
    float dy = (float)(g_lastMouseY - ypos) * MOUSE_SENS;   // عكس Y محور الشاشة
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    // تحديث زوايا الكاميرا.
    g_player.yaw   += dx;
    g_player.pitch += dy;
    // Clamp على pitch فقط (yaw يدور 360° بلا حدود).
    if (g_player.pitch >  85.0f) g_player.pitch =  85.0f;
    if (g_player.pitch < -85.0f) g_player.pitch = -85.0f;
}

// ----------------------------------------------------------------------------
//  [AR] framebufferSizeCallback : دالة استدعاء عند تغيير حجم النافذة
//  ----------------------------------------------------------------------------
//  يجب تحديث Viewport لـ OpenGL ليطابق حجم النافذة الجديد، وإلا ستظهر الرسومات مشوّهة.
//  نحفظ SCR_W / SCR_H لاستخدامهما في حساب نسبة العرض/الارتفاع لمصفوفة الإسقاط وإحداثيات HUD.
// ----------------------------------------------------------------------------
static void framebufferSizeCallback(GLFWwindow*, int w, int h)
{
    SCR_W = (unsigned int)w;
    SCR_H = (unsigned int)h;
    glViewport(0, 0, w, h);    // تحديث نطاق العرض في OpenGL.
}

// ----------------------------------------------------------------------------
//  [AR] processInput : قراءة الكيبورد وتطبيق الحركة
//  ----------------------------------------------------------------------------
//  تُستدعى كل إطار داخل الحلقة الرئيسيّة :
//    1) ESC     → طلب إغلاق النافذة.
//    2) R (حواف) → إعادة بناء المستوى (إعادة اللعبة). نستخدم كشف حافة (edge) :
//         ننفّذ عند الانتقال من غير مضغوط إلى مضغوط فقط (ليس كل إطار طوال الضغط).
//    3) إن كانت الحالة ليست Playing → لا حركة (توقف التحكم باللاعب في حالتي Won/Lost).
//    4) بناء متجهي الحركة الأساسيّين :
//         forward = مسقوط اتجاه الكاميرا على مستوى XZ (نتجاهل pitch لنمنع الطيران).
//         right   = cross(forward, worldUp) = المتجه المتعامد على اليمين.
//    5) تجميع متجه الحركة حسب الأزرار المضغوطة ثمّ ضربه في PLAYER_SPEED × deltaTime.
//    6) الانزلاق بفصل المحاور :
//         نجرّب حركة X أولاً، إن اصطدمت نلغيها وحدها. ثمّ نجرّب Z. هذا يجعل اللاعب
//         ينزلق على الجدار بدل الالتصاق.
//
//  ضرب الحركة في g_deltaTime ضروري : يجعل السرعة مستقلّة عن معدل الإطارات (FPS).
// ----------------------------------------------------------------------------
static void processInput(GLFWwindow* window)
{
    // (1) خروج بـ ESC.
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    // (2) إعادة اللعبة بـ R مع كشف حافة (مرة واحدة فقط عند الضغط).
    static bool rPrev = false;
    bool rNow = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
    if (rNow && !rPrev) buildLevel();
    rPrev = rNow;

    // (3) إن انتهت اللعبة (فوز/خسارة) لا نسمح بالحركة.
    if (g_state != GameState::Playing) return;

    // (4) بناء متجه الأمام في مستوى XZ فقط (نتجاهل pitch لمنع الطيران).
    glm::vec3 forward(
        cos(glm::radians(g_player.yaw)),
        0.0f,
        sin(glm::radians(g_player.yaw))
    );
    // متجه اليمين = الضرب المتجه (forward × worldUp).
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    // (5) تجميع متجه الحركة حسب الأزرار (W/A/S/D).
    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += forward;   // تقدّم
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= forward;   // رجوع
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;     // يمين
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;     // يسار

    if (glm::length(move) > 0.001f) {
        g_isMoving = true;
        // تطبيع ثمّ ضرب في السرعة × deltaTime → حركة مستقلّة عن FPS.
        move = glm::normalize(move) * PLAYER_SPEED * g_deltaTime;

        // (6) الانزلاق بفصل المحاور (Axis-Separated Sliding Collision).
        // جرّب X أولاً :
        glm::vec3 np = g_player.pos;
        np.x += move.x;
        if (!collides(np)) g_player.pos.x = np.x;     // لا تصادم → طبّق حركة X.
        // جرّب Z ثانياً :
        np = g_player.pos;
        np.z += move.z;
        if (!collides(np)) g_player.pos.z = np.z;     // لا تصادم → طبّق حركة Z.
    } else {
        g_isMoving = false;    // لا حركة → أوقف تمايل الفانوس.
    }
}

// ============================================================================
//  [AR] updateGame : تحديث منطق اللعبة (منفصل عن الرسم)
//  ----------------------------------------------------------------------------
//  فصل المنطق عن الرسم يُلبّي نص الزوج الأول : "فصل تحديث المنطق عن الرسم".
//  هذه الدالة تُغيّر حالة اللعبة فقط؛ دوال الرسم في حلقة main() تقرأ هذه الحالة فقط دون تعديل.
//
//  ماذا تفعل كل إطار :
//    1) إن انتهت اللعبة (فوز/خسارة) → تخرج فوراً (لا تنقيص مؤقّت ولا التقاط عملات).
//    2) فحص التقاط العملات :
//         - المسافة بين اللاعب والعملة في مستوى XZ فقط (لا نهتمّ بارتفاع Y).
//         - إن كانت أصغر من 0.8 وحدة (مربّعة = 0.64) → التقاط العملة.
//         - ضرب spin في g_deltaTime يضمن دوراناً ثابت السرعة بفصرف النظر عن FPS.
//    3) تحديث طور تمايل الفانوس فقط حين يتحرّك اللاعب.
//    4) خصم المؤقّت وفحص شروط النهاية :
//         إن g_coinsLeft <= 0 → اللاعب جمع الكل → Won.
//         إن انتهى المؤقّت → Lost.
// ============================================================================
static void updateGame()
{
    // (1) لا نحدّث المنطق بعد النهاية (Won/Lost) → تجميد الحالة.
    if (g_state != GameState::Playing) return;

    // (2) فحص الالتقاط (المسافة في مستوى XZ فقط).
    for (Coin& c : g_coins) {
        if (!c.alive) continue;                       // تجاهل العملات المجموعة.
        c.spin += g_deltaTime * 2.5f;                 // دوران مستقلّ عن FPS (2.5 راديان/ثانية).
        float dx = c.pos.x - g_player.pos.x;
        float dz = c.pos.z - g_player.pos.z;
        if (dx * dx + dz * dz < 0.8f * 0.8f) {        // مسافة تربيعيّة < 0.64
            c.alive = false;                          // رفع علم الالتقاط (لن تُرسم بعدها).
            --g_coinsLeft;                            // خصم عدّاد العملات الحيّة.
            std::cout << "Coin collected! Remaining: " << g_coinsLeft << "\n";
        }
    }

    // (3) تحديث طور تمايل المشي (8 راديان/ثانية) — يستخدمه نموذج الفانوس أدناه.
    if (g_isMoving) g_walkPhase += g_deltaTime * 8.0f;

    // (4) خصم المؤقّت وفحص شروط النهاية.
    g_timeLeft -= g_deltaTime;
    if (g_coinsLeft <= 0)   g_state = GameState::Won;                       // جمع الكل → فوز
    else if (g_timeLeft <= 0.0f) { g_timeLeft = 0.0f; g_state = GameState::Lost; }  // انتهى الوقت → خسارة
}

// ============================================================================
//  [AR] دوال مساعدة لضبط الـ Uniforms (متغيّرات من CPU إلى شيدرات GPU)
//  ----------------------------------------------------------------------------
//  الـ Uniform هو متغيّر غير قابل للتحرير داخل الشيدر، يُضبط من طرف الـ CPU
//  ويكون موحّداً لكل رأس/فراقمنت داخل نفس استدعاء الرسم.
//
//  الخطوات لضبط Uniform :
//    1) glGetUniformLocation(p, name) : يبحث عن موقع المتغيّر داخل البرنامج.
//    2) glUniform*f / glUniformMatrix* : يرسل القيمة.
//  نكتب هذه الدوال المختصرة لتقليل الضجيج في حلقة الرسم.
//
//  ملاحظة أداء : المشاريع الكبيرة تحفظ الـ Location في كاش بدل طلبه في كل إطار.
// ============================================================================
static void setVec3 (unsigned int p, const char* n, const glm::vec3& v) { glUniform3fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setVec4 (unsigned int p, const char* n, const glm::vec4& v) { glUniform4fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setVec2 (unsigned int p, const char* n, const glm::vec2& v) { glUniform2fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setMat4 (unsigned int p, const char* n, const glm::mat4& m) { glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, glm::value_ptr(m)); }
static void setInt  (unsigned int p, const char* n, int i)              { glUniform1i (glGetUniformLocation(p, n), i); }
static void setFloat(unsigned int p, const char* n, float f)            { glUniform1f (glGetUniformLocation(p, n), f); }

// ============================================================================
//  [AR] main() : نقطة بدء التشغيل
//  ----------------------------------------------------------------------------
//  تتكوّن من أربع مراحل رئيسيّة :
//    (أ) التهيئة : GLFW و GLEW و حالة OpenGL.
//    (ب) تحميل الموارد : الشيدرات والخامات والأشكال.
//    (ج) الحلقة الرئيسيّة : إدخال → تحديث → رسم → تبديل المخزّنين.
//    (د) التنظيف : حذف الموارد وإغلاق GLFW.
// ============================================================================
int main()
{
    // ========================================================================
    //  [AR] (أ) تهيئة البيئة — الزوج الأول
    // ========================================================================

    // تهيئة مكتبة GLFW (تحميل دوال النظام + تهيئة الإدخال).
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return -1; }

    // طلب OpenGL 3.3 Core Profile :
    //   3.3      : أصغر إصدار مدعوم على كل التعريفات الحديثة.
    //   Core     : يحذف الخطّ الثابت القديم ويفرض شيدرات صريحة (مطلوب للمعايير!).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // إنشاء النافذة وسياق OpenGL المرتبط بها.
    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Rama - 3D Maze Collector", nullptr, nullptr);
    if (!window) { std::cerr << "glfwCreateWindow failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);                              // جعل سياق النافذة هو الحالي.

    // تسجيل دوال الاستدعاء (حجم النافذة + الماوس) + إخفاء مؤشّر الماوس.
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // قفل المؤشّر في النافذة.

    // تهيئة GLEW بعد إنشاء السياق — يبحث عن مؤشّرات دوال OpenGL الحديثة.
    //  glewExperimental = GL_TRUE : يجبر GLEW على تحميل مداخل Core Profile التي يتخطّاها افتراضياً.
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "glewInit failed\n"; return -1; }

    // تفعيل خيارات OpenGL الضروريّة :
    glEnable(GL_DEPTH_TEST);      // اختبار العمق → الأقرب يحجب الأبعد.
    glEnable(GL_BLEND);           // المزج → لدعم الشفافيّة (طبقة HUD وحواف العملة).
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // معادلة "over" القياسيّة.

    // ========================================================================
    //  [AR] (ب) تحميل الموارد (شيدرات + خامات + أشكال)
    // ========================================================================

    // برنامجا الشيدر : واحد للعالم الثلاثي والثاني لـ HUD الثنائي.
    unsigned int worldProg = makeProgram("Shaders/world.vert", "Shaders/world.frag");
    unsigned int hudProg   = makeProgram("Shaders/hud.vert",   "Shaders/hud.frag");

    // خامتان : الجدران/الأرضية + العملات.
    unsigned int texWall = loadTexture("Assets/container.jpg");
    unsigned int texCoin = loadTexture("Assets/awesomeface.png");

    // بناء الأشكال الأربعة : مكعّب + أرضية (بـ EBO) + مربّع عملة + مربّع HUD.
    unsigned int cubeVBO, floorVBO, floorEBO, quadVBO, hudVBO;
    unsigned int cubeVAO  = makeCubeVAO (cubeVBO);
    unsigned int floorVAO = makeFloorVAO(floorVBO, floorEBO, (MAZE_W * CELL) * 0.5f, (float)MAZE_W);
    unsigned int quadVAO  = makeQuadVAO (quadVBO);
    unsigned int hudVAO   = makeHudQuadVAO(hudVBO);

    // تحويل شبكة MAZE إلى جدران + عملات + موقع اللاعب.
    buildLevel();

    // ثوابت الإضاءة والضباب (ثابتة طوال اللعبة) :
    const glm::vec3 FOG_COLOR(0.55f, 0.62f, 0.70f);   // لون رمادي-أزرق بارد.
    const float     FOG_DENSITY = 0.045f;             // كثافة الضباب الأُسّي.
    const glm::vec3 LIGHT_DIR(-0.4f, -1.0f, -0.3f);   // اتجاه الضوء (شمس خياليّة فوق المشهد).

    // ========================================================================
    //  [AR] (ج) الحلقة الرئيسيّة — تستمر حتى يغلق المستخدم النافذة
    //  ------------------------------------------------------------------------
    //  ترتيب كل إطار (مهمّ جداً) :
    //    1) حساب g_deltaTime = الزمن منذ الإطار السابق → يجعل السرعة مستقلّة عن FPS.
    //    2) glfwPollEvents : يستقبل أحداث النظام ويستدعي mouseCallback وغيره.
    //    3) processInput : الكيبورد + حركة اللاعب.
    //    4) updateGame   : تحديث المنطق (عملات ، مؤقّت ، فوز/خسارة).
    //    5) مسح الشاشة (اللون والعمق).
    //    6) حساب مصفوفات الكاميرا (View + Projection).
    //    7) رسم العالم : أرضية → جدران → فانوس → عملات.
    //    8) رسم HUD (بعد إيقاف اختبار العمق).
    //    9) glfwSwapBuffers : تبديل المخزّن الأمامي مع الخلفي (إظهار الإطار المرسوم).
    // ========================================================================
    while (!glfwWindowShouldClose(window)) {
        // (1) حساب زمن الإطار (deltaTime) مع حدّ أقصى لتجنّب القفزات الكبيرة.
        float now = (float)glfwGetTime();
        g_deltaTime = now - g_lastFrame;
        g_lastFrame = now;
        if (g_deltaTime > 0.05f) g_deltaTime = 0.05f; // وقف القفزات (مثلاً عند تعليق النافذة)

        // (2–4) استقبال الأحداث → الإدخال → تحديث المنطق.
        glfwPollEvents();
        processInput(window);
        updateGame();

        // (5) مسح المخزّنين : اللون = لون الضباب (ليوحي بعمق المشهد) + مخزّن العمق.
        glClearColor(FOG_COLOR.r, FOG_COLOR.g, FOG_COLOR.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --------------------------------------------------------------------
        //  [AR] (6) حساب مصفوفات الكاميرا — الزوج الثالث (كاميرا منظوريّة)
        // --------------------------------------------------------------------
        //  متجه الأمام (front) مبني من إحداثيات كرويّة :
        //    front.x = cos(yaw) × cos(pitch)
        //    front.y = sin(pitch)
        //    front.z = sin(yaw) × cos(pitch)
        //  هذه الصيغة تُحول زاويتين (yaw, pitch) إلى متجه وحدة يشير حيث ينظر اللاعب.
        glm::vec3 front(
            cos(glm::radians(g_player.yaw)) * cos(glm::radians(g_player.pitch)),
            sin(glm::radians(g_player.pitch)),
            sin(glm::radians(g_player.yaw)) * cos(glm::radians(g_player.pitch))
        );
        glm::vec3 eye  = g_player.pos;
        //  مصفوفة الرؤية : تنقل العالم من الفضاء العالمي إلى فضاء الكاميرا.
        //    lookAt(eye, target, up) : target = eye + front (نقطة أمام اللاعب).
        glm::mat4 view = glm::lookAt(eye, eye + glm::normalize(front), glm::vec3(0, 1, 0));
        //  مصفوفة الإسقاط المنظوريّ :
        //    FOV = 60° (مجال الرؤية العمودي).
        //    aspect = عرض/ارتفاع النافذة.
        //    near = 0.1  و far = 80  → مجال التصوير.
        glm::mat4 proj = glm::perspective(glm::radians(60.0f),
            (float)SCR_W / (float)std::max(1u, SCR_H), 0.1f, 80.0f);

        // --------------------------------------------------------------------
        //  [AR] (7) مرحلة العالم (3D) — تفعيل برنامج الشيدر وضبط Uniforms المشتركة
        // --------------------------------------------------------------------
        glUseProgram(worldProg);   // تفعيل برنامج شيدر العالم.
        // إرسال الـ Uniforms الموحّدة لكل شيء يُرسم في العالم :
        setMat4 (worldProg, "uView",       view);          // مصفوفة الرؤية
        setMat4 (worldProg, "uProjection", proj);          // مصفوفة الإسقاط
        setVec3 (worldProg, "uLightDir",   LIGHT_DIR);     // اتجاه الضوء
        setVec3 (worldProg, "uFogColor",   FOG_COLOR);     // لون الضباب
        setFloat(worldProg, "uFogDensity", FOG_DENSITY);   // كثافة الضباب
        setInt  (worldProg, "uTex",        0);             // وحدة الخامة 0 (GL_TEXTURE0)
        setInt  (worldProg, "uDiscardAlpha", 0);           // إيقاف اختبار الألفا افتراضياً

        glActiveTexture(GL_TEXTURE0);   // جعل وحدة الخامة 0 هي المستهدفة.

        // --- (7أ) رسم الأرضية (حرج : برهان استخدام EBO + glDrawElements) ------
        glBindTexture(GL_TEXTURE_2D, texWall);
        setInt (worldProg, "uUseTexture", 1);                          // نفعل أخذ اللون من الخامة.
        setVec3(worldProg, "uTint", glm::vec3(0.85f, 0.85f, 0.85f));   // تخفيف السطوع قليلاً.
        setMat4(worldProg, "uModel", glm::mat4(1.0f));                 // مصفوفة النموذج = هوية (الأرضية في مكانها).
        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);           // 6 فهارس → مثلّثان.

        // --- (7ب) رسم الجدران — الزوج 6 (عوائق تشكل المتاهة) -------
        setVec3(worldProg, "uTint", glm::vec3(0.95f, 0.85f, 0.75f));   // لون بني دافئ للجدران.
        glBindVertexArray(cubeVAO);
        //  لكل جدار نبني مصفوفة Model خاصة (نقل + تكبير) ونستدعي glDrawArrays.
        //  هذا يسمّى Instancing اليدوي (لو كانت الجدران كثيرة جداً لاستخدمنا glDrawArraysInstanced).
        for (const glm::vec3& w : g_walls) {
            glm::mat4 m(1.0f);
            m = glm::translate(m, w);                                  // نقل المكعّب لمركز الجدار.
            m = glm::scale(m, glm::vec3(CELL, 2.0f, CELL));            // تكبير إلى حجم الخلية.
            setMat4(worldProg, "uModel", m);
            glDrawArrays(GL_TRIANGLES, 0, 36);                         // 36 رأساً = 6 أوجه.
        }

        // --- (7ج) رسم الفانوس المحمول — الزوج 5 (نموذج شخصية 3D مرئي) --
        //  مكعّب صغير برونزي يطفو أمام-يمين الكاميرا ويتمايل أثناء المشي.
        //  نستخدم نفس مكعّب الجدران لكن بمصفوفة نموذج مشتقّة من متجهات الكاميرا.
        {
            //  متجه الأمام "المسطّح" (بدون Y) لأن الفانوس يجب أن يبقى أفقيّاً حتى لو نظر اللاعب للأعلى.
            glm::vec3 upV(0.0f, 1.0f, 0.0f);
            glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
            glm::vec3 rightV    = glm::normalize(glm::cross(flatFront, upV));
            //  تمايل رأسي (bob) يستخدم طور المشي g_walkPhase.
            float     bob       = g_isMoving ? sin(g_walkPhase) * 0.04f : 0.0f;
            //  موقع الفانوس = موقع العين + إزاحة للأمام واليمين والأسفل.
            glm::vec3 lanternPos = eye
                + flatFront * 0.55f                               // 55 سنتم أمام العين
                + rightV    * 0.35f                               // 35 سنتم يميناً
                + glm::vec3(0.0f, -0.40f + bob, 0.0f);            // 40 سنتم أسفل العين + تمايل

            //  بناء مصفوفة Model : نقل → دوران مع الكاميرا → دوران دائم → تصغير.
            glm::mat4 m(1.0f);
            m = glm::translate(m, lanternPos);                          // (1) نقل لموضع اليد.
            m = glm::rotate   (m, -glm::radians(g_player.yaw), upV);    // (2) دوران مع yaw للكاميرا.
            m = glm::rotate   (m,  now * 1.5f, glm::vec3(0, 1, 0));     // (3) دوران مستمرّ للحياة البصريّة.
            m = glm::scale    (m, glm::vec3(0.18f));                    // (4) تصغير إلى 18% من حجم المكعّب.

            setMat4(worldProg, "uModel", m);
            setInt (worldProg, "uUseTexture", 0);                       // بدون خامة → لون من uTint.
            setVec3(worldProg, "uTint", glm::vec3(1.0f, 0.72f, 0.25f)); // برونزي دافئ.
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // --- (7د) رسم العملات — الزوج 7 (عناصر قابلة للجمع) ---------
        glBindTexture(GL_TEXTURE_2D, texCoin);            // خامة الوجه المبتسم (بشفافية).
        setInt  (worldProg, "uUseTexture",   1);
        setInt  (worldProg, "uDiscardAlpha", 1);          // تفعيل اختبار الألفا → حذف البكسلات الشفّافة.
        setVec3 (worldProg, "uTint", glm::vec3(1.0f));
        glDisable(GL_CULL_FACE);                          // إيقاف الـ culling → العملة ثنائية الوجه.
        glBindVertexArray(quadVAO);
        //  لكل عملة حيّة : نبني مصفوفة Model (نقل + دوران حول Y + تكبير).
        for (const Coin& c : g_coins) {
            if (!c.alive) continue;                                 // تجاهل المجموعة.
            glm::mat4 m(1.0f);
            m = glm::translate(m, c.pos);                           // موقع العملة.
            m = glm::rotate   (m, c.spin, glm::vec3(0, 1, 0));      // دوران حول المحور العمودي.
            m = glm::scale    (m, glm::vec3(1.5f));                 // تكبير 50% لتظهر واضحة.
            setMat4(worldProg, "uModel", m);
            glDrawArrays(GL_TRIANGLES, 0, 6);                       // 6 رؤوس = مثلّثان.
        }
        setInt(worldProg, "uDiscardAlpha", 0);             // إعادة القيمة الافتراضيّة.

        // ====================================================================
        //  [AR] (8) مرحلة HUD — الزوج 7 (واجهة على الشاشة)
        //  --------------------------------------------------------------------
        //  الخطوات :
        //    - إيقاف اختبار العمق : هذه طبقة 2D فوق المشهد، يجب أن تغطيه دائماً.
        //    - تبديل إلى برنامج شيدر HUD.
        //    - بناء مصفوفة إسقاط أرثوغرافي (إحداثيات بكسل).
        //    - رسم مربّعات العملات وشريط المؤقّت وطبقة الفوز/الخسارة.
        //    - إعادة تفعيل اختبار العمق للإطار التالي.
        // ====================================================================
        glDisable(GL_DEPTH_TEST);                                           // طبقة 2D تعلو كل شيء.
        glUseProgram(hudProg);
        //  مصفوفة إسقاط أرثوغرافي : (0,0) الركن السفلي-الأيسر، (SCR_W,SCR_H) السفلي-الأيمن.
        glm::mat4 ortho = glm::ortho(0.0f, (float)SCR_W, 0.0f, (float)SCR_H);
        setMat4(hudProg, "uOrtho", ortho);
        glBindVertexArray(hudVAO);

        //  دالة داخليّة لرسم مربّع HUD ملوّن (إحداثيات بكسل ولون RGBA).
        auto drawHudQuad = [&](float x, float y, float w, float h, glm::vec4 col) {
            setVec2(hudProg, "uOffset", glm::vec2(x, y));
            setVec2(hudProg, "uSize",   glm::vec2(w, h));
            setVec4(hudProg, "uColor",  col);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        };

        // --- (8أ) أيقونات العملات في الركن العلوي-الأيسر -------------
        //  5 مربّعات صغيرة : ذهبية إن كانت العملة حيّة، رماديّة باهتة إن جُمعت.
        const float pad = 16.0f, sz = 28.0f, gap = 8.0f;
        for (size_t i = 0; i < g_coins.size(); ++i) {
            float x = pad + (float)i * (sz + gap);
            float y = (float)SCR_H - pad - sz;                      // من الأعلى (محور Y لـ ortho يبدأ من الأسفل).
            glm::vec4 col = g_coins[i].alive
                ? glm::vec4(1.0f, 0.82f, 0.1f, 1.0f)                // ذهبي
                : glm::vec4(0.25f, 0.25f, 0.25f, 0.6f);             // رمادي باهت
            drawHudQuad(x, y, sz, sz, col);
        }

        // --- (8ب) شريط المؤقّت في الوسط العلوي -----------------------
        //  خلفية سوداء نصف شفّافة + داخلها شريط ملوّن بنسبة الزمن المتبقّي.
        //  اللون يتدرّج : أخضر > أصفر > أحمر حسب نسبة الزمن المتبقّي.
        {
            float barW = 320.0f, barH = 14.0f;
            float x = ((float)SCR_W - barW) * 0.5f;
            float y = (float)SCR_H - pad - barH;
            drawHudQuad(x, y, barW, barH, glm::vec4(0, 0, 0, 0.55f));           // خلفية.
            float frac = glm::clamp(g_timeLeft / GAME_TIME, 0.0f, 1.0f);        // نسبة الزمن [0..1].
            glm::vec4 c = frac > 0.5f ? glm::vec4(0.2f, 0.9f, 0.3f, 0.95f)      // > 50% → أخضر
                        : frac > 0.2f ? glm::vec4(0.95f, 0.8f, 0.2f, 0.95f)     // > 20% → أصفر
                                      : glm::vec4(0.95f, 0.25f, 0.2f, 0.95f);   // إلّا → أحمر
            drawHudQuad(x + 2, y + 2, (barW - 4) * frac, barH - 4, c);          // الشريط الداخلي.
        }

        // --- (8ج) طبقة الفوز/الخسارة على كامل الشاشة -------------
        //  عند الفوز : غطاء أخضر شفّاف + شريط أخضر أدخن في الوسط.
        //  عند الخسارة : غطاء أحمر + شريط أحمر.
        if (g_state == GameState::Won) {
            drawHudQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.2f, 0.8f, 0.3f, 0.35f));
            drawHudQuad((float)SCR_W * 0.25f, (float)SCR_H * 0.45f,
                (float)SCR_W * 0.5f, (float)SCR_H * 0.1f,
                glm::vec4(0.1f, 0.6f, 0.2f, 0.9f));
        }
        else if (g_state == GameState::Lost) {
            drawHudQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.8f, 0.15f, 0.15f, 0.4f));
            drawHudQuad((float)SCR_W * 0.25f, (float)SCR_H * 0.45f,
                (float)SCR_W * 0.5f, (float)SCR_H * 0.1f,
                glm::vec4(0.6f, 0.1f, 0.1f, 0.9f));
        }

        glEnable(GL_DEPTH_TEST);                      // إعادة تفعيل اختبار العمق للإطار التالي.

        // (9) تبديل المخزّنين (Double Buffering) : نسخ ما رسمناه إلى الشاشة.
        //  الرسم يتمّ على مخزّن خلفي (غير مرئي) لتجنّب ظهور إطارات غير مكتملة.
        glfwSwapBuffers(window);
    }

    // ========================================================================
    //  [AR] (د) التنظيف — حذف الموارد بترتيب عكسي لإنشائها
    //  ------------------------------------------------------------------------
    //  تقنيّاً : نظام التشغيل يعيد أيّ موارد GPU متبقّية بعد الخروج، لكن :
    //    - التنظيف الصريح أمر حسن.
    //    - مطلوب في المشاريع التي قد تشغل وتغلق النوافذ مراراً دون أن تنتهي العملية.
    //    - سؤال شائع في المناقشة!
    // ========================================================================
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteVertexArrays(1, &hudVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &floorEBO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &hudVBO);
    glDeleteTextures(1, &texWall);
    glDeleteTextures(1, &texCoin);
    glDeleteProgram(worldProg);
    glDeleteProgram(hudProg);

    glfwTerminate();
    return 0;
}
