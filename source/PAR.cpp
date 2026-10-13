// ============================================================================
//  PAR  -  Precision Approach Radar scopes for EuroScope (France military/joint airfields)
//  Version 0.2.0
//
//  One floating window with an AZIMUTH scope (plan view: left/right of the extended
//  centreline) and an ELEVATION scope (side view: above/below the glidepath), plus
//  a panel with the numbers and a suggested talk-down transmission.
//
//  The window is a normal Windows window (it can be moved, resized, put on another
//  monitor). It follows the aircraft selected in EuroScope (ASEL) unless told otherwise.
//
//  Commands (EuroScope command line):
//    .par rwy LFRJ 07      choose airfield + runway (opens the window)
//    .par show | hide      show / hide the window
//    .par list             list the runways of PAR.csv
//    .par track CALLSIGN   fix the scope on one aircraft      .par track asel  = follow selection
//    .par range 10         scope range in NM (2 - 20)
//    .par az 500           azimuth scope half-height in metres (50 - 3000)
//    .par el 500           elevation scope half-height in feet (50 - 3000)
//    .par qnh 1013         QNH used to correct the pressure altitude
//    .par dh 200           decision height above threshold, ft
//    .par top on | off     keep the window above everything
//    .par others on | off  show / hide other traffic in the approach cone (no argument = toggle)
//    .par clear            stop tracking (same as .par track off)
//  DESELECT AIRCRAFT button (info panel): release the tracked aircraft until another one is selected.
//    .par reload           re-read PAR.ini and PAR.csv
//    .par status
//
//  Build: same recipe as WxOverlay (MFC DLL, x86, Release, static MFC, /MT, one source file).
// ============================================================================
#include <afxwin.h>
#include <math.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include "EuroScopePlugIn.h"

using namespace EuroScopePlugIn;

#define PAR_VERSION "0.2.3"
static const double PI_ = 3.14159265358979323846;
static const double EARTH_R = 6371008.8;      // m
static const double M_PER_NM = 1852.0;
static const double FT_PER_M = 3.280839895;
static const double TDZ_M = 300.0;            // touchdown point = 300 m after the threshold

// ----------------------------------------------------------------------------
class CParApp : public CWinApp {
public:
    virtual BOOL InitInstance() { CWinApp::InitInstance(); return TRUE; }
};
CParApp theApp;

// ----------------------------------------------------------------------------
struct Rwy {
    std::string icao, rwy;
    double lat, lon;        // threshold, decimal degrees
    double elevFt;          // threshold elevation
    double trueCourse;      // runway/final course, true
    double magCourse;       // runway/final course, magnetic (what is told to the pilot)
    double gsDeg;           // glidepath angle
    double rdhFt;           // reference datum height over the threshold
    std::string note;
};

struct Tgt {
    std::string cs;
    double dist;            // m before the threshold (negative = past it)
    double cross;           // m, positive = right of the centreline (looking along the landing direction)
    double dev;             // ft above (+) / below (-) the glidepath
    double alt;             // ft, QNH corrected
    int gs;                 // kt
    bool sel;
};

struct TrailPt { double t; double dist, cross, dev; };

// ----------------------------------------------------------------------------
static std::string ModuleDir()
{
    HMODULE h = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&ModuleDir, &h);
    char p[MAX_PATH] = { 0 };
    GetModuleFileNameA(h, p, MAX_PATH);
    std::string s(p);
    size_t k = s.find_last_of("\\/");
    return (k == std::string::npos) ? std::string(".") : s.substr(0, k);
}

static std::string Upper(std::string s) { for (size_t i = 0; i < s.size(); i++) s[i] = (char)toupper((unsigned char)s[i]); return s; }
static double Norm360(double a) { a = fmod(a, 360.0); if (a < 0) a += 360.0; return a; }
static int Hdg(double a) { int h = (int)floor(Norm360(a) + 0.5); if (h == 0) h = 360; return h; }
static int Round50(double v) { return (int)(floor(v / 50.0 + 0.5) * 50.0); }

// ----------------------------------------------------------------------------
class CParPlugIn;
static CParPlugIn* g_plugin = NULL;
static LRESULT CALLBACK ScopeProc(HWND, UINT, WPARAM, LPARAM);

class CParPlugIn : public CPlugIn {
public:
    CParPlugIn() : CPlugIn(COMPATIBILITY_CODE, "PAR", PAR_VERSION, "PAR scopes", "free to use")
    {
        g_plugin = this;
        hwnd = NULL; haveRwy = false; follow = true; topmost = true; showOthers = false;
        SetRectEmpty(&btnRelease);
        rangeNm = 10.0; azHalfM = 500.0; elHalfFt = 500.0; qnh = 1013.25; dhFt = 200.0;
        hasSel = false; selDist = selCross = selDev = selAlt = 0; selGs = 0; lastPlotTime = 0;
        LoadAll(true);
        Msg(std::string("PAR ") + PAR_VERSION + " loaded - " + std::to_string((long long)rwys.size()) +
            " runways. Use .par rwy <ICAO> <RWY>");
    }
    ~CParPlugIn()
    {
        if (hwnd) { DestroyWindow(hwnd); hwnd = NULL; }
        g_plugin = NULL;
    }

    // ---- messages -------------------------------------------------------
    void Msg(const std::string& m)
    {
        DisplayUserMessage("PAR", "PAR", m.c_str(), true, true, true, false, false);
    }

    // ---- loading --------------------------------------------------------
    void LoadAll(bool first)
    {
        std::string dir = ModuleDir();
        // CSV
        rwys.clear();
        FILE* f = fopen((dir + "\\PAR_Runways.csv").c_str(), "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                std::string s(line);
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                if (s.empty() || s[0] == '#') continue;
                std::vector<std::string> p;
                size_t a = 0;
                for (;;) { size_t b = s.find(';', a); if (b == std::string::npos) { p.push_back(s.substr(a)); break; } p.push_back(s.substr(a, b - a)); a = b + 1; }
                if (p.size() < 9) continue;
                Rwy r;
                r.icao = Upper(p[0]); r.rwy = Upper(p[1]);
                r.lat = atof(p[2].c_str()); r.lon = atof(p[3].c_str());
                r.elevFt = atof(p[4].c_str()); r.trueCourse = atof(p[5].c_str());
                r.magCourse = atof(p[6].c_str()); r.gsDeg = atof(p[7].c_str());
                r.rdhFt = atof(p[8].c_str());
                r.note = p.size() > 9 ? p[9] : "";
                rwys.push_back(r);
            }
            fclose(f);
        } else {
            Msg("PAR_Runways.csv not found next to the DLL (" + dir + ")");
        }
        // INI
        std::string ini = dir + "\\PAR.ini";
        char b[128];
        rangeNm = GetPrivateProfileIntA("PAR", "RangeNM", (int)rangeNm, ini.c_str());
        azHalfM = GetPrivateProfileIntA("PAR", "AzHalfM", (int)azHalfM, ini.c_str());
        elHalfFt = GetPrivateProfileIntA("PAR", "ElHalfFt", (int)elHalfFt, ini.c_str());
        dhFt = GetPrivateProfileIntA("PAR", "DecisionHeightFt", (int)dhFt, ini.c_str());
        topmost = GetPrivateProfileIntA("PAR", "TopMost", 1, ini.c_str()) != 0;
        showOthers = GetPrivateProfileIntA("PAR", "ShowOthers", 0, ini.c_str()) != 0;
        GetPrivateProfileStringA("PAR", "QNH", "1013", b, sizeof(b), ini.c_str());
        qnh = atof(b); if (qnh < 900 || qnh > 1100) qnh = 1013.25;
        if (first) {
            GetPrivateProfileStringA("PAR", "Runway", "", b, sizeof(b), ini.c_str());
            std::string rw = b;
            size_t sp = rw.find(' ');
            if (sp != std::string::npos) SelectRunway(Upper(rw.substr(0, sp)), Upper(rw.substr(sp + 1)), false);
        } else if (haveRwy) {
            SelectRunway(cur.icao, cur.rwy, false);
        }
    }

    bool SelectRunway(const std::string& icao, const std::string& rw, bool show)
    {
        for (size_t i = 0; i < rwys.size(); i++)
            if (rwys[i].icao == icao && rwys[i].rwy == rw) {
                cur = rwys[i]; haveRwy = true; trail.clear(); trailCs.clear();
                if (show) ShowWnd(true);
                Refresh();
                return true;
            }
        return false;
    }

    // ---- window ---------------------------------------------------------
    void EnsureWnd()
    {
        if (hwnd) return;
        static bool reg = false;
        HINSTANCE hi = GetModuleHandleA(NULL);
        if (!reg) {
            WNDCLASSEXA wc; memset(&wc, 0, sizeof(wc));
            wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = ScopeProc; wc.hInstance = hi;
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = NULL; wc.lpszClassName = "EsParScopeWnd";
            RegisterClassExA(&wc);
            reg = true;
        }
        hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | (topmost ? WS_EX_TOPMOST : 0), "EsParScopeWnd",
                               "PAR scope", WS_OVERLAPPEDWINDOW, 80, 80, 1200, 760, NULL, NULL, hi, NULL);
    }
    void ShowWnd(bool s)
    {
        EnsureWnd();
        if (!hwnd) return;
        ShowWindow(hwnd, s ? SW_SHOWNOACTIVATE : SW_HIDE);
        SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        UpdateTitle();
        if (s) InvalidateRect(hwnd, NULL, FALSE);
    }
    void UpdateTitle()
    {
        if (!hwnd) return;
        std::string t = "PAR scope";
        if (haveRwy) t += "  -  " + cur.icao + " RWY " + cur.rwy;
        SetWindowTextA(hwnd, t.c_str());
    }

    // ---- geometry -------------------------------------------------------
    void Project(double lat, double lon, double& dist, double& cross) const
    {
        double e = (lon - cur.lon) * PI_ / 180.0 * EARTH_R * cos(cur.lat * PI_ / 180.0);
        double n = (lat - cur.lat) * PI_ / 180.0 * EARTH_R;
        double th = cur.trueCourse * PI_ / 180.0;
        double s = e * sin(th) + n * cos(th);           // along the landing direction, + = past the threshold
        double c = e * cos(th) - n * sin(th);           // + = right of the centreline
        dist = -s; cross = c;
    }
    double IdealAlt(double distM) const
    {
        return cur.elevFt + cur.rdhFt + (distM * FT_PER_M) * tan(cur.gsDeg * PI_ / 180.0);
    }

    // ---- periodic update (EuroScope calls this every second) --------------
    virtual void OnTimer(int Counter)
    {
        (void)Counter;
        if (!haveRwy) return;
        Refresh();
    }

    void Refresh()
    {
        if (!haveRwy) return;
        tgts.clear();
        std::string want;
        if (follow) {
            CRadarTarget a = RadarTargetSelectASEL();
            std::string asel = a.IsValid() ? a.GetCallsign() : "";
            if (asel != releasedCs) releasedCs.clear();          // a new selection cancels the release
            if (releasedCs.empty()) want = asel;
        }
        else want = fixedCs;
        hasSel = false;
        for (CRadarTarget rt = RadarTargetSelectFirst(); rt.IsValid(); rt = RadarTargetSelectNext(rt)) {
            CRadarTargetPositionData pd = rt.GetPosition();
            if (!pd.IsValid()) continue;
            double dist, cross;
            Project(pd.GetPosition().m_Latitude, pd.GetPosition().m_Longitude, dist, cross);
            double rng = rangeNm * M_PER_NM;
            bool isSel = (!want.empty() && want == rt.GetCallsign());
            if (!isSel) {
                if (!showOthers) continue;
                if (dist > rng || dist < -1500.0) continue;
                if (fabs(cross) > (std::max)(1500.0, fabs(dist) * 0.3)) continue;
            }
            double alt = pd.GetPressureAltitude() + (qnh - 1013.25) * 27.0;
            Tgt t; t.cs = rt.GetCallsign(); t.dist = dist; t.cross = cross;
            t.alt = alt; t.dev = alt - IdealAlt(dist); t.gs = pd.GetReportedGS(); t.sel = isSel;
            tgts.push_back(t);
            if (isSel) {
                hasSel = true; sel = t;
                if (trailCs != t.cs) { trail.clear(); trailCs = t.cs; }
                double now = (double)GetTickCount() / 1000.0;
                if (trail.empty() || now - trail.back().t > 0.8) {
                    TrailPt p; p.t = now; p.dist = dist; p.cross = cross; p.dev = t.dev;
                    trail.push_back(p);
                    if (trail.size() > 24) trail.pop_front();
                }
            }
        }
        if (!hasSel) { trail.clear(); trailCs.clear(); }
        if (hwnd && IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
    }

    // ---- suggestions --------------------------------------------------------
    struct Advice { std::string distTxt, azDesc, azAct, elDesc, elAct, full, dhTxt; int hdgToFly; double azDevM; double elDevAng; };

    Advice MakeAdvice() const
    {
        Advice a; a.hdgToFly = Hdg(cur.magCourse); a.azDevM = sel.cross; a.dhTxt.clear();
        char buf[256];
        double dTdz = sel.dist + TDZ_M;
        double dft = (std::max)(sel.dist, 50.0) * FT_PER_M;
        a.elDevAng = atan2(sel.dev, dft) * 180.0 / PI_;

        // distance
        if (sel.dist < 0) a.distTxt = "Over the threshold";
        else { snprintf(buf, sizeof(buf), "%.1f miles from touchdown", (std::max)(0.0, dTdz) / M_PER_NM); a.distTxt = buf; }

        // cross-track rate from the trail
        double vcross = 0; bool haveRate = false;
        if (trail.size() >= 3) {
            const TrailPt& p0 = trail[trail.size() >= 5 ? trail.size() - 5 : 0];
            const TrailPt& p1 = trail.back();
            double dt = p1.t - p0.t;
            if (dt > 2.0) { vcross = (p1.cross - p0.cross) / dt; haveRate = true; }
        }
        double L = sel.cross, aL = fabs(L);
        const char* side = (L > 0) ? "right" : "left";
        if (aL < 25) a.azDesc = "On centreline";
        else if (aL < 75) a.azDesc = std::string("Slightly ") + side + " of centreline";
        else if (aL < 200) a.azDesc = std::string(side) + " of centreline";
        else a.azDesc = std::string("Well ") + side + " of centreline";
        if (!a.azDesc.empty()) a.azDesc[0] = (char)toupper((unsigned char)a.azDesc[0]);

        bool converging = haveRate && (L * vcross < 0);
        double tcross = (converging && fabs(vcross) > 0.05) ? aL / fabs(vcross) : 1e9;
        if (aL < 25) {
            snprintf(buf, sizeof(buf), "maintain heading %03d", a.hdgToFly); a.azAct = buf;
        } else if (converging && tcross < 40.0) {
            snprintf(buf, sizeof(buf), "converging, maintain present heading"); a.azAct = buf;
        } else {
            double ang = atan2(aL, (std::max)(sel.dist, 800.0)) * 180.0 / PI_;
            int corr = (int)floor(ang * 3.0 + 0.5);
            if (corr < 2) corr = 2;
            if (corr > 12) corr = 12;
            if (aL > 600) corr = (std::max)(corr, 8);
            int h = Hdg(cur.magCourse + (L > 0 ? -corr : corr));
            snprintf(buf, sizeof(buf), "turn %s %d degrees, heading %03d", (L > 0) ? "left" : "right", corr, h);
            a.azAct = buf; a.hdgToFly = h;
        }

        // elevation
        double ang = a.elDevAng, aa = fabs(ang);
        double fpmNominal = sel.gs * 101.27 * tan(cur.gsDeg * PI_ / 180.0);
        const char* ud = (ang > 0) ? "above" : "below";
        if (aa < 0.2) a.elDesc = "On glidepath";
        else if (aa < 0.5) a.elDesc = std::string("Slightly ") + ud + " glidepath";
        else if (aa < 1.0) a.elDesc = std::string(ud) + " glidepath";
        else a.elDesc = std::string("Well ") + ud + " glidepath";
        a.elDesc[0] = (char)toupper((unsigned char)a.elDesc[0]);
        if (sel.gs < 30) { a.elAct = "(speed not available)"; }
        else if (aa < 0.2) { snprintf(buf, sizeof(buf), "maintain rate of descent, about %d feet per minute", Round50(fpmNominal)); a.elAct = buf; }
        else if (ang > 0) {
            double f = (aa < 0.5) ? 1.2 : (aa < 1.0 ? 1.5 : 2.0);
            snprintf(buf, sizeof(buf), "increase rate of descent, about %d feet per minute", Round50(fpmNominal * f)); a.elAct = buf;
        } else {
            if (aa >= 1.0) a.elAct = "level off, check altitude";
            else { double f = (aa < 0.5) ? 0.7 : 0.4; snprintf(buf, sizeof(buf), "reduce rate of descent, about %d feet per minute", Round50(fpmNominal * f)); a.elAct = buf; }
        }

        double h = sel.alt - cur.elevFt;
        if (h <= dhFt) a.dhTxt = "AT / BELOW DECISION HEIGHT";
        else if (h <= dhFt + 100) a.dhTxt = "Approaching decision height";

        a.full = a.distTxt + ". " + a.azDesc + ", " + a.azAct + ". " + a.elDesc + ", " + a.elAct + ".";
        if (!a.dhTxt.empty()) a.full += "  [" + a.dhTxt + "]";
        return a;
    }

    // ---- drawing --------------------------------------------------------------
    void Paint(HDC hdcScreen, RECT rc)
    {
        int W = rc.right - rc.left, H = rc.bottom - rc.top;
        if (W < 200 || H < 150) return;
        HDC dc = CreateCompatibleDC(hdcScreen);
        HBITMAP bm = CreateCompatibleBitmap(hdcScreen, W, H);
        HGDIOBJ oldBm = SelectObject(dc, bm);
        SetBkMode(dc, TRANSPARENT);

        HBRUSH bg = CreateSolidBrush(RGB(8, 14, 12));
        RECT full = { 0, 0, W, H };
        FillRect(dc, &full, bg); DeleteObject(bg);

        HFONT fSmall = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
        HFONT fBig = CreateFontA(18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
        HGDIOBJ oldF = SelectObject(dc, fSmall);

        if (!haveRwy) {
            SetTextColor(dc, RGB(200, 220, 200));
            RECT r = { 20, 20, W - 20, H - 20 };
            DrawTextA(dc, "No runway selected.\n\nType  .par list  then  .par rwy LFRJ 07", -1, &r, DT_LEFT | DT_WORDBREAK);
        } else {
            // left column: info panel on top, elevation scope below; right column: vertical azimuth scope
            int m = 8, labelL = 56, labelB = 18, azLabelL = 30;
            int azW = (std::max)(260, W * 30 / 100);
            int colR = W - m - azW;
            int infoH = (std::min)(240, (H - 3 * m) / 2);
            RECT el = { m + labelL, 2 * m + infoH, colR, H - m - labelB };
            RECT az = { colR + m + azLabelL, m, W - m, H - m - labelB };
            DrawInfo(dc, m, m, colR - m, infoH, fSmall, fBig);
            DrawScope(dc, el, false);
            DrawScope(dc, az, true);
        }
        SelectObject(dc, oldF);
        DeleteObject(fSmall); DeleteObject(fBig);
        BitBlt(hdcScreen, rc.left, rc.top, W, H, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBm); DeleteObject(bm); DeleteDC(dc);
    }

    // Scope coordinates. distM = metres before the threshold, v = deviation (m or ft), half = scope half-size.
    // Elevation: horizontal, range on the left, threshold on the right, up = above the glidepath.
    // Azimuth:   vertical, threshold at the top, range at the bottom, right = right of the centreline.
    POINT Map(double distM, double v, double half, const RECT& r, bool azimuth) const
    {
        double rng = rangeNm * M_PER_NM, lo = -600.0;       // far edge = range, near edge = 600 m past the threshold
        double f = (rng - distM) / (rng - lo);
        double k = v / half;
        POINT p;
        if (azimuth) {
            double cx = (r.left + r.right) / 2.0, hw = (r.right - r.left) / 2.0;
            p.x = (int)(cx + k * hw);
            p.y = r.bottom - (int)(f * (r.bottom - r.top));
        } else {
            double cy = (r.top + r.bottom) / 2.0, hh = (r.bottom - r.top) / 2.0;
            p.x = r.left + (int)(f * (r.right - r.left));
            p.y = (int)(cy - k * hh);
        }
        return p;
    }
    static void Line(HDC dc, POINT a, POINT b) { MoveToEx(dc, a.x, a.y, NULL); LineTo(dc, b.x, b.y); }
    static int TextW(HDC dc, const char* s) { SIZE z; GetTextExtentPoint32A(dc, s, (int)strlen(s), &z); return z.cx; }

    void DrawScope(HDC dc, const RECT& r, bool azimuth)
    {
        HPEN pFrame = CreatePen(PS_SOLID, 1, RGB(70, 110, 90));
        HPEN pGrid = CreatePen(PS_DOT, 1, RGB(30, 55, 45));
        HPEN pCentre = CreatePen(PS_SOLID, 1, RGB(120, 220, 160));
        HPEN pLimit = CreatePen(PS_DASH, 1, RGB(200, 160, 40));
        HPEN pThr = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
        HGDIOBJ op = SelectObject(dc, pFrame);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HGDIOBJ ob = SelectObject(dc, nb);
        Rectangle(dc, r.left, r.top, r.right, r.bottom);

        double half = azimuth ? azHalfM : elHalfFt;
        double rng = rangeNm * M_PER_NM, lo = -600.0;
        // range grid + labels (every NM, or every 2 / 5 NM on long ranges)
        int step = (rangeNm > 12) ? 5 : (rangeNm > 6 ? 2 : 1);
        SetTextColor(dc, RGB(140, 180, 160));
        for (int n = 0; n <= (int)rangeNm; n += step) {
            POINT a = Map(n * M_PER_NM, -half, half, r, azimuth), b = Map(n * M_PER_NM, half, half, r, azimuth);
            SelectObject(dc, pGrid); Line(dc, a, b);
            char t[16]; snprintf(t, sizeof(t), "%d", n);
            if (azimuth) TextOutA(dc, r.left - 6 - TextW(dc, t), a.y - 7, t, (int)strlen(t));
            else TextOutA(dc, a.x - TextW(dc, t) / 2, r.bottom + 2, t, (int)strlen(t));
        }
        // deviation grid (thirds)
        for (int k = -3; k <= 3; k++) {
            if (k == 0) continue;
            double v = half * k / 3.0;
            POINT a = Map(rng, v, half, r, azimuth), b = Map(lo, v, half, r, azimuth);
            SelectObject(dc, pGrid); Line(dc, a, b);
            char t[24];
            if (azimuth) {
                snprintf(t, sizeof(t), "%d%s", (int)floor(fabs(v) + 0.5), k < 0 ? "L" : "R");
                TextOutA(dc, a.x - TextW(dc, t) / 2, r.bottom + 2, t, (int)strlen(t));
            } else {
                snprintf(t, sizeof(t), "%+d ft", (int)floor(v + 0.5));
                TextOutA(dc, r.left - 54, a.y - 7, t, (int)strlen(t));
            }
        }
        // centreline / glidepath
        SelectObject(dc, pCentre); Line(dc, Map(rng, 0, half, r, azimuth), Map(lo, 0, half, r, azimuth));
        // limits: azimuth +/-1 deg, elevation +/-0.5 deg
        SelectObject(dc, pLimit);
        double limDeg = azimuth ? 1.0 : 0.5;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            for (int i = 0; i <= 60; i++) {
                double d = rng - (rng - lo) * i / 60.0;
                double lim = (std::max)(d, 0.0) * tan(limDeg * PI_ / 180.0) * (azimuth ? 1.0 : FT_PER_M) * sgn;
                lim = (std::max)(-half, (std::min)(half, lim));
                POINT p = Map(d, lim, half, r, azimuth);
                if (i == 0) MoveToEx(dc, p.x, p.y, NULL); else LineTo(dc, p.x, p.y);
            }
        }
        // threshold marker
        SelectObject(dc, pThr);
        POINT t0 = Map(0, -half, half, r, azimuth), t1 = Map(0, half, half, r, azimuth);
        Line(dc, t0, t1);
        SetTextColor(dc, RGB(230, 230, 230));
        if (azimuth) TextOutA(dc, r.left + 4, t0.y + 2, "THR", 3);
        else TextOutA(dc, t0.x - 12, r.top + 2, "THR", 3);
        if (!azimuth) {   // minimum line: decision height expressed against the glidepath
            double dFt = (dhFt - cur.rdhFt) / tan(cur.gsDeg * PI_ / 180.0);     // distance (ft) where the glidepath is at DH
            int x = Map(dFt / FT_PER_M, 0, half, r, false).x;
            SelectObject(dc, pLimit); MoveToEx(dc, x, r.top, NULL); LineTo(dc, x, r.bottom);
            SetTextColor(dc, RGB(200, 160, 40)); TextOutA(dc, x + 3, r.bottom - 16, "DH", 2);
        }
        // title
        SetTextColor(dc, RGB(150, 230, 180));
        if (azimuth) {
            TextOutA(dc, r.left + 6, r.bottom - 32, "AZIMUTH", 7);       // at the bottom, clear of THR
            TextOutA(dc, r.left + 6, r.bottom - 18, "right = right of CL (m)", 23);
        } else {
            const char* title = "ELEVATION  (up = above glidepath)";
            TextOutA(dc, r.left + 6, r.top + 16, title, (int)strlen(title));
        }
        // trail of the selected target
        if (hasSel) {
            HBRUSH tb = CreateSolidBrush(RGB(90, 200, 120));
            for (size_t i = 0; i < trail.size(); i++) {
                POINT p = Map(trail[i].dist, azimuth ? trail[i].cross : trail[i].dev, half, r, azimuth);
                if (p.x < r.left || p.x > r.right || p.y < r.top || p.y > r.bottom) continue;
                RECT d = { p.x - 1, p.y - 1, p.x + 2, p.y + 2 }; FillRect(dc, &d, tb);
            }
            DeleteObject(tb);
        }
        // targets (outside the range axis: not drawn; outside the deviation axis: pinned to the edge)
        for (size_t i = 0; i < tgts.size(); i++) {
            const Tgt& t = tgts[i];
            POINT p = Map(t.dist, azimuth ? t.cross : t.dev, half, r, azimuth);
            bool clipped;
            if (azimuth) {
                if (p.y < r.top || p.y > r.bottom) continue;
                clipped = (p.x < r.left || p.x > r.right);
                if (p.x < r.left) p.x = r.left + 3; if (p.x > r.right) p.x = r.right - 3;
            } else {
                if (p.x < r.left || p.x > r.right) continue;
                clipped = (p.y < r.top || p.y > r.bottom);
                if (p.y < r.top) p.y = r.top + 3; if (p.y > r.bottom) p.y = r.bottom - 3;
            }
            COLORREF col = t.sel ? RGB(255, 255, 90) : RGB(120, 160, 140);
            HBRUSH b = CreateSolidBrush(col);
            int s = t.sel ? 5 : 3;
            RECT d = { p.x - s, p.y - s, p.x + s + 1, p.y + s + 1 };
            FillRect(dc, &d, b); DeleteObject(b);
            SetTextColor(dc, col);
            std::string lab = t.cs; if (clipped) lab += " (off scale)";
            int lx = p.x + 8, lw = TextW(dc, lab.c_str());
            if (lx + lw > r.right) lx = p.x - 8 - lw;        // keep the label inside the scope
            TextOutA(dc, lx, p.y - 7, lab.c_str(), (int)lab.size());
        }
        SelectObject(dc, op); SelectObject(dc, ob);
        DeleteObject(pFrame); DeleteObject(pGrid); DeleteObject(pCentre); DeleteObject(pLimit); DeleteObject(pThr);
    }

    // Info panel: settings on the left, aircraft numbers on the right, suggested transmission across the bottom.
    void DrawInfo(HDC dc, int x, int y, int w, int h, HFONT fSmall, HFONT fBig)
    {
        RECT box = { x, y, x + w, y + h };
        HPEN p = CreatePen(PS_SOLID, 1, RGB(70, 110, 90));
        HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, box.left, box.top, box.right, box.bottom);
        SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(p);

        int lx = x + 10, rx = x + w / 2, ly = y + 8, ry = y + 8; char b[256];
        SelectObject(dc, fBig);
        SetTextColor(dc, RGB(150, 230, 180));
        snprintf(b, sizeof(b), "%s RWY %s", cur.icao.c_str(), cur.rwy.c_str());
        TextOutA(dc, lx, ly, b, (int)strlen(b)); ly += 26;
        SelectObject(dc, fSmall);
        SetTextColor(dc, RGB(170, 200, 185));
        snprintf(b, sizeof(b), "Course %03d M  G/P %.1f  RDH %d ft", Hdg(cur.magCourse), cur.gsDeg, (int)cur.rdhFt);
        TextOutA(dc, lx, ly, b, (int)strlen(b)); ly += 17;
        snprintf(b, sizeof(b), "Range %.0f NM  QNH %.0f  DH %d ft", rangeNm, qnh, (int)dhFt);
        TextOutA(dc, lx, ly, b, (int)strlen(b)); ly += 17;
        snprintf(b, sizeof(b), "Tracking: %s   Others: %s", follow ? "selected aircraft" : (fixedCs.empty() ? "none" : fixedCs.c_str()),
                 showOthers ? "on" : "off");
        TextOutA(dc, lx, ly, b, (int)strlen(b)); ly += 17;

        SetRectEmpty(&btnRelease);
        if (!hasSel) {
            SetTextColor(dc, RGB(230, 180, 90));
            RECT r = { rx, ry, x + w - 10, y + h - 10 };
            std::string rel = "Released " + releasedCs + ".\n\nSelect another aircraft in EuroScope to track it.";
            DrawTextA(dc, follow ? (!releasedCs.empty() ? rel.c_str()
                                    : "No aircraft selected, or the selected aircraft is not on this runway's approach.\n\nSelect an aircraft in EuroScope.")
                                 : fixedCs.empty() ? "Tracking off.\n\n.par track asel  to follow the selection again."
                                 : "Aircraft not found.", -1, &r, DT_LEFT | DT_WORDBREAK);
            return;
        }
        Advice a = MakeAdvice();
        // "DESELECT AIRCRAFT" button, top right of the panel
        {
            const char* bt = "DESELECT AIRCRAFT";
            int bw = TextW(dc, bt) + 20;
            SetRect(&btnRelease, x + w - 10 - bw, y + 8, x + w - 10, y + 30);
            HBRUSH bb = CreateSolidBrush(RGB(30, 50, 42));
            HPEN bp = CreatePen(PS_SOLID, 1, RGB(120, 220, 160));
            HGDIOBJ obp = SelectObject(dc, bp), obb = SelectObject(dc, bb);
            Rectangle(dc, btnRelease.left, btnRelease.top, btnRelease.right, btnRelease.bottom);
            SelectObject(dc, obp); SelectObject(dc, obb); DeleteObject(bp); DeleteObject(bb);
            SetTextColor(dc, RGB(150, 230, 180));
            RECT tr = btnRelease;
            DrawTextA(dc, bt, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dc, fBig);
        SetTextColor(dc, RGB(255, 255, 110));
        TextOutA(dc, rx, ry, sel.cs.c_str(), (int)sel.cs.size()); ry += 26;
        SelectObject(dc, fSmall);
        SetTextColor(dc, RGB(210, 235, 220));
        snprintf(b, sizeof(b), "Distance THR  %.1f NM", sel.dist / M_PER_NM);
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;
        snprintf(b, sizeof(b), "Groundspeed   %d kt", sel.gs);
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;
        snprintf(b, sizeof(b), "Altitude      %d ft (QNH)", (int)floor(sel.alt + 0.5));
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;
        snprintf(b, sizeof(b), "Azimuth       %d m %s", (int)floor(fabs(sel.cross) + 0.5), sel.cross > 0 ? "RIGHT" : "LEFT");
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;
        snprintf(b, sizeof(b), "Elevation     %+d ft (%+.2f deg)", (int)floor(sel.dev + 0.5), a.elDevAng);
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;
        snprintf(b, sizeof(b), "Heading to fly %03d M", a.hdgToFly);
        TextOutA(dc, rx, ry, b, (int)strlen(b)); ry += 17;

        int ty = (std::max)(ly, ry) + 8;
        if (!a.dhTxt.empty()) {
            SetTextColor(dc, RGB(255, 140, 90));
            TextOutA(dc, lx, ly + 8, a.dhTxt.c_str(), (int)a.dhTxt.size());      // fits under the settings column
        }
        SetTextColor(dc, RGB(150, 230, 180));
        TextOutA(dc, lx, ty, "Suggested transmission:", 23); ty += 18;
        SetTextColor(dc, RGB(240, 240, 200));
        RECT r = { lx, ty, x + w - 10, y + h - 6 };
        DrawTextA(dc, a.full.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK);
    }

    // ---- commands ---------------------------------------------------------------
    virtual bool OnCompileCommand(const char* sCommandLine)
    {
        std::string c = sCommandLine;
        if (c.size() < 4 || Upper(c.substr(0, 4)) != ".PAR") return false;
        if (c.size() > 4 && c[4] != ' ') return false;
        std::vector<std::string> t;
        { size_t i = 0; while (i < c.size()) { while (i < c.size() && c[i] == ' ') i++; size_t j = i; while (j < c.size() && c[j] != ' ') j++; if (j > i) t.push_back(c.substr(i, j - i)); i = j; } }
        std::string cmd = t.size() > 1 ? Upper(t[1]) : "STATUS";
        std::string a1 = t.size() > 2 ? t[2] : "";
        if (cmd == "RWY" && t.size() >= 4) {
            if (!SelectRunway(Upper(t[2]), Upper(t[3]), true)) Msg("Unknown runway. Type .par list");
            else Msg("PAR: " + cur.icao + " RWY " + cur.rwy);
        } else if (cmd == "SHOW") { ShowWnd(true); }
        else if (cmd == "HIDE") { ShowWnd(false); }
        else if (cmd == "LIST") {
            std::string cur_icao, line;
            for (size_t i = 0; i < rwys.size(); i++) {
                if (rwys[i].icao != cur_icao) { if (!line.empty()) Msg(line); cur_icao = rwys[i].icao; line = cur_icao + ":"; }
                line += " " + rwys[i].rwy;
            }
            if (!line.empty()) Msg(line);
        } else if (cmd == "TRACK" && !a1.empty()) {
            std::string u = Upper(a1);
            if (u == "ASEL") { follow = true; releasedCs.clear(); Msg("PAR: following the selected aircraft"); }
            else if (u == "OFF" || u == "NONE") { follow = false; fixedCs.clear(); Msg("PAR: tracking off"); }
            else { follow = false; fixedCs = u; Msg("PAR: tracking " + fixedCs); }
            trail.clear(); Refresh();
        } else if (cmd == "CLEAR") {
            follow = false; fixedCs.clear(); trail.clear(); Refresh(); Msg("PAR: tracking off (.par track asel to follow the selection again)");
        } else if (cmd == "OTHERS") {
            std::string u = Upper(a1);
            showOthers = (u == "ON") ? true : (u == "OFF") ? false : !showOthers;
            Refresh(); Msg(std::string("PAR: other traffic ") + (showOthers ? "shown" : "hidden"));
        } else if (cmd == "RANGE" && !a1.empty()) { rangeNm = (std::min)(20.0, (std::max)(2.0, atof(a1.c_str()))); Refresh(); }
        else if (cmd == "AZ" && !a1.empty()) { azHalfM = (std::min)(3000.0, (std::max)(50.0, atof(a1.c_str()))); Refresh(); }
        else if (cmd == "EL" && !a1.empty()) { elHalfFt = (std::min)(3000.0, (std::max)(50.0, atof(a1.c_str()))); Refresh(); }
        else if (cmd == "QNH" && !a1.empty()) { double q = atof(a1.c_str()); if (q > 900 && q < 1100) { qnh = q; Refresh(); } else Msg("PAR: QNH out of range"); }
        else if (cmd == "DH" && !a1.empty()) { dhFt = (std::min)(1000.0, (std::max)(0.0, atof(a1.c_str()))); Refresh(); }
        else if (cmd == "TOP") { topmost = (Upper(a1) != "OFF"); if (hwnd) SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
        else if (cmd == "RELOAD") { LoadAll(false); Msg("PAR: reloaded, " + std::to_string((long long)rwys.size()) + " runways"); }
        else {
            char b[256];
            snprintf(b, sizeof(b), "PAR %s | %s | range %.0f NM | az +/-%.0f m | el +/-%.0f ft | QNH %.0f | DH %.0f | track %s | others %s",
                     PAR_VERSION, haveRwy ? (cur.icao + " " + cur.rwy).c_str() : "no runway", rangeNm, azHalfM, elHalfFt, qnh, dhFt,
                     follow ? "ASEL" : (fixedCs.empty() ? "off" : fixedCs.c_str()), showOthers ? "on" : "off");
            Msg(b);
        }
        return true;
    }

    // state shared with the window procedure
    HWND hwnd;
    std::vector<Rwy> rwys;
    Rwy cur; bool haveRwy;
    bool follow, topmost, showOthers; std::string fixedCs;
    std::string releasedCs;     // ASEL released with the DESELECT button; ignored until the selection changes
    RECT btnRelease;            // DESELECT AIRCRAFT button, set while painting (empty when not shown)

    // click in the window: only the DESELECT button does something
    void OnWndClick(int x, int y)
    {
        POINT pt = { x, y };
        if (!PtInRect(&btnRelease, pt)) return;
        // release the tracked aircraft and go back to following the selection
        CRadarTarget a = RadarTargetSelectASEL();
        releasedCs = a.IsValid() ? a.GetCallsign() : "";
        follow = true; fixedCs.clear(); trail.clear();
        Refresh();
    }
    double rangeNm, azHalfM, elHalfFt, qnh, dhFt;
    std::vector<Tgt> tgts; bool hasSel; Tgt sel;
    std::deque<TrailPt> trail; std::string trailCs;
    double selDist, selCross, selDev, selAlt, lastPlotTime; int selGs;
};

// ----------------------------------------------------------------------------
static LRESULT CALLBACK ScopeProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        if (g_plugin) g_plugin->Paint(dc, rc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CLOSE: ShowWindow(h, SW_HIDE); return 0;     // hide only; .par show brings it back
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: if (g_plugin) g_plugin->OnWndClick((short)LOWORD(l), (short)HIWORD(l)); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

// ----------------------------------------------------------------------------
void __declspec(dllexport) EuroScopePlugInInit(CPlugIn** ppPlugInInstance)
{
    *ppPlugInInstance = new CParPlugIn();
}

void __declspec(dllexport) EuroScopePlugInExit(void)
{
    if (g_plugin) delete g_plugin;
}
