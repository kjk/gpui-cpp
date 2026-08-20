/* crates/story/src/fixtures: the JSON chart_story.rs loads at startup.
   An example here has no JSON reader and no file to read, so the four
   fixtures that story uses are transcribed, values and all. */

// daily-devices.json: 91 days of device counts.
static const int kDailyDeviceCount = 91;
static const char* const kDailyDate[] = {
    "Apr 1",  "Apr 2",  "Apr 3",  "Apr 4",  "Apr 5",  "Apr 6",  "Apr 7",
    "Apr 8",  "Apr 9",  "Apr 10", "Apr 11", "Apr 12", "Apr 13", "Apr 14",
    "Apr 15", "Apr 16", "Apr 17", "Apr 18", "Apr 19", "Apr 20", "Apr 21",
    "Apr 22", "Apr 23", "Apr 24", "Apr 25", "Apr 26", "Apr 27", "Apr 28",
    "Apr 29", "Apr 30", "May 1",  "May 2",  "May 3",  "May 4",  "May 5",
    "May 6",  "May 7",  "May 8",  "May 9",  "May 10", "May 11", "May 12",
    "May 13", "May 14", "May 15", "May 16", "May 17", "May 18", "May 19",
    "May 20", "May 21", "May 22", "May 23", "May 24", "May 25", "May 26",
    "May 27", "May 28", "May 29", "May 30", "May 31", "Jun 1",  "Jun 2",
    "Jun 3",  "Jun 4",  "Jun 5",  "Jun 6",  "Jun 7",  "Jun 8",  "Jun 9",
    "Jun 10", "Jun 11", "Jun 12", "Jun 13", "Jun 14", "Jun 15", "Jun 16",
    "Jun 17", "Jun 18", "Jun 19", "Jun 20", "Jun 21", "Jun 22", "Jun 23",
    "Jun 24", "Jun 25", "Jun 26", "Jun 27", "Jun 28", "Jun 29", "Jun 30"};
static const float kDailyDesktop[] = {
    222.f, 97.f,  167.f, 242.f, 373.f, 301.f, 245.f, 409.f, 59.f,  261.f, 327.f,
    292.f, 342.f, 137.f, 120.f, 138.f, 446.f, 364.f, 243.f, 89.f,  137.f, 224.f,
    138.f, 387.f, 215.f, 75.f,  383.f, 122.f, 315.f, 454.f, 165.f, 293.f, 247.f,
    385.f, 481.f, 498.f, 388.f, 149.f, 227.f, 293.f, 335.f, 197.f, 197.f, 448.f,
    473.f, 338.f, 499.f, 315.f, 235.f, 177.f, 82.f,  81.f,  252.f, 294.f, 201.f,
    213.f, 420.f, 233.f, 78.f,  340.f, 178.f, 178.f, 470.f, 103.f, 439.f, 88.f,
    294.f, 323.f, 385.f, 438.f, 155.f, 92.f,  492.f, 81.f,  426.f, 307.f, 371.f,
    475.f, 107.f, 341.f, 408.f, 169.f, 317.f, 480.f, 132.f, 141.f, 434.f, 448.f,
    149.f, 103.f, 446.f};
static const float kDailyMobile[] = {
    111.f, 48.f,  84.f,  121.f, 187.f, 151.f, 123.f, 205.f, 30.f,  131.f, 164.f,
    146.f, 171.f, 69.f,  60.f,  69.f,  223.f, 182.f, 122.f, 44.f,  69.f,  112.f,
    69.f,  194.f, 108.f, 38.f,  192.f, 61.f,  158.f, 227.f, 82.f,  146.f, 124.f,
    192.f, 241.f, 249.f, 194.f, 74.f,  114.f, 146.f, 168.f, 98.f,  98.f,  224.f,
    236.f, 169.f, 250.f, 158.f, 118.f, 88.f,  41.f,  41.f,  126.f, 147.f, 100.f,
    106.f, 210.f, 116.f, 39.f,  170.f, 89.f,  89.f,  235.f, 52.f,  220.f, 44.f,
    147.f, 162.f, 192.f, 219.f, 78.f,  46.f,  246.f, 41.f,  213.f, 154.f, 186.f,
    238.f, 54.f,  171.f, 204.f, 84.f,  158.f, 240.f, 66.f,  70.f,  217.f, 224.f,
    74.f,  52.f,  223.f};
static const float kDailyTablet[] = {
    67.f,  29.f,  50.f,  73.f,  112.f, 91.f,  74.f,  123.f, 18.f,  79.f,  98.f,
    88.f,  103.f, 41.f,  36.f,  41.f,  134.f, 109.f, 73.f,  26.f,  41.f,  67.f,
    41.f,  116.f, 65.f,  23.f,  115.f, 37.f,  95.f,  136.f, 49.f,  88.f,  74.f,
    115.f, 145.f, 149.f, 116.f, 44.f,  68.f,  88.f,  101.f, 59.f,  59.f,  134.f,
    142.f, 101.f, 150.f, 95.f,  71.f,  53.f,  25.f,  25.f,  76.f,  88.f,  60.f,
    64.f,  126.f, 70.f,  23.f,  102.f, 53.f,  53.f,  141.f, 31.f,  132.f, 26.f,
    88.f,  97.f,  115.f, 131.f, 47.f,  28.f,  148.f, 25.f,  128.f, 92.f,  112.f,
    143.f, 32.f,  103.f, 122.f, 50.f,  95.f,  144.f, 40.f,  42.f,  130.f, 134.f,
    44.f,  31.f,  134.f};
static const float kDailyWatch[] = {
    28.f, 12.f, 21.f, 30.f, 47.f, 38.f, 31.f, 51.f, 8.f,  33.f, 41.f, 36.f,
    43.f, 17.f, 15.f, 17.f, 56.f, 46.f, 30.f, 11.f, 17.f, 28.f, 17.f, 48.f,
    27.f, 10.f, 48.f, 15.f, 40.f, 57.f, 20.f, 36.f, 31.f, 48.f, 60.f, 62.f,
    48.f, 18.f, 28.f, 36.f, 42.f, 24.f, 24.f, 56.f, 59.f, 42.f, 62.f, 40.f,
    30.f, 22.f, 10.f, 10.f, 32.f, 37.f, 25.f, 26.f, 52.f, 29.f, 10.f, 42.f,
    22.f, 22.f, 59.f, 13.f, 55.f, 11.f, 37.f, 40.f, 48.f, 55.f, 20.f, 12.f,
    62.f, 10.f, 53.f, 38.f, 46.f, 60.f, 14.f, 43.f, 51.f, 21.f, 40.f, 60.f,
    16.f, 18.f, 54.f, 56.f, 18.f, 13.f, 56.f};

// monthly-devices.json: six months, each with its own color alpha.
static const int kMonthlyDeviceCount = 6;
static const char* const kMonthlyMonth[] = {"Jan",   "Feb", "March",
                                            "April", "May", "June"};
static const float kMonthlyDesktop[] = {186.f, 305.f, 237.f,
                                        73.f,  209.f, 214.f};
static const float kMonthlyAlpha[] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f};

// radar-devices.json.
static const int kRadarDeviceCount = 6;
static const char* const kRadarMonth[] = {"January", "February", "March",
                                          "April",   "May",      "June"};
static const float kRadarDesktop[] = {186.f, 305.f, 237.f, 73.f, 209.f, 214.f};
static const float kRadarMobile[] = {80.f, 200.f, 120.f, 190.f, 130.f, 140.f};

// stock-prices.json.
static const int kStockPriceCount = 6;
static const char* const kStockDate[] = {"Jan", "Feb", "Mar",
                                         "Apr", "May", "Jun"};
static const float kStockOpen[] = {100.f, 110.f, 111.f, 116.f, 110.f, 115.f};
static const float kStockHigh[] = {112.f, 112.f, 118.f, 120.f, 118.f, 125.f};
static const float kStockLow[] = {95.f, 108.f, 110.f, 108.f, 105.f, 113.f};
static const float kStockClose[] = {110.f, 111.f, 116.f, 110.f, 115.f, 123.f};
