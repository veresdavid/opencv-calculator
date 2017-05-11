#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <cmath>

#include "calculator.h"

using namespace cv;
using namespace std;

// állandó értékek
const int FPS = 30;
const int WIDTH = 640;
const int HEIGHT = 480;
const int TRANSX = 100;
const int TRANSY = 0;
const int BOX_SIZE = 13;
const Rect MAIN_RECT(0, 0, 3.0 / 4.0 * WIDTH,/* 3.0 / 4.0 * */HEIGHT);
const double PI = 3.14159265358979323846;
const Scalar DOWN(12, 30, 80);
const Scalar UP(7, 40, 80);

// színek
const Scalar GREEN(0, 255, 0);

// webkamera
VideoCapture videoCapture(0);

// képek
Mat frame;
Mat frame_hsv;
Mat mask;

// a kéz színének mintavétlezéséhez használt változók
vector<Point> sample_points;
vector<Rect> sample_rects;
vector<Scalar> sample_medians;
bool sample_mode = true;

// a kéz meghatározásához és események kezeléséhez használt változók
vector<Point> finger_points;
vector<Point> fingertips_before;
vector<Point> fingertips_actual;
vector<bool> palm_is_opens;
double average_radius;
float radius;
double last_avg;
double decrease_percent = 0.2;
vector<double> radius_samples;
int fps_counter = 0;
bool running = true;
int counter = 0;

// számológép
calculator calc(1.2, 60, 25);

void on_mouse_click(int, int, int, int, void*);

// a kéz színének mintavételezéséhez használt változók inicializálása
void initSamplePoints() {

	// mintavézelezési téglalapok bal felsõ sarkait tartalmazó vektor kiürítése
	sample_points.clear();

	// mintavételezési téglalapok bal felsõ sarkának inicializálása
	sample_points.push_back(Point(WIDTH / 2 - TRANSX, HEIGHT / 2 - 50 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - 25 - TRANSX, HEIGHT / 2 - 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 + 25 - TRANSX, HEIGHT / 2 - 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - TRANSX, HEIGHT / 2 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - 25 - TRANSX, HEIGHT / 2 + 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 + 25 - TRANSX, HEIGHT / 2 + 25 - TRANSY));

	// mintavételezési téglalapok létrehozásas
	for (int i = 0; i < sample_points.size(); i++) {
		sample_rects.push_back(Rect(sample_points[i], Size(BOX_SIZE, BOX_SIZE)));
	}

}

// a mintavételezéshez használt téglalapok kirajzolása a cél képre
void drawSampleRects(Mat& dest) {

	for (int i = 0; i < sample_rects.size(); i++) {
		rectangle(dest, sample_rects[i], GREEN);
	}

}

// uchar típusú vektor elemeinek mediánjának meghatározása
uchar median(vector<uchar>& values) {

	// elemek növekvõ sorrendbe rendezése
	sort(values.begin(), values.end());

	// páros darabszámú vektor esetén a két középsõ elem átlaga a medián
	if (values.size() % 2 == 0) {
		return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2;
	}
	// páratlan darabszámú vektor esetén a középsõ elem a medián
	else {
		return values[values.size() / 2];
	}

}

// kép színeinek mediánjának meghatározása
Scalar getMedianOfMat(const Mat& src) {

	vector<uchar> h;
	vector<uchar> s;
	vector<uchar> v;

	for (int r = 0; r < src.rows; r++) {
		for (int c = 0; c < src.cols; c++) {

			Vec3b color = src.at<Vec3b>(r, c);

			h.push_back(color[0]);
			s.push_back(color[1]);
			v.push_back(color[2]);

		}
	}

	return Scalar(median(h), median(s), median(v));

}

// a mintavételezési téglalapokból a mediánok kinyerése
void getSampleMedians() {

	for (int i = 0; i < sample_rects.size(); i++) {

		// a mintavételezési téglalapból kihagyjuk a zöld keretet, ehhez új jobb felsõ és bal alsó sarkak kiválasztása
		Point top_left = sample_rects[i].tl() + Point(1, 1);
		Point bottom_right = sample_rects[i].br() - Point(1, 1);

		// medianBlur szûrõ alkalmazása a szükséges részre
		Mat area = frame_hsv(Rect(top_left, bottom_right));
		// medianBlur(frame_hsv(Rect(top_left, bottom_right)), area, BOX_SIZE);

		// medián hozzáfûzése a listához
		sample_medians.push_back(getMedianOfMat(area));

	}

}

// a mintavételezési téglalapokból a mediánok kinyerése
void getSampleMedians_2() {

	for (int i = 0; i < sample_rects.size(); i++) {

		Mat tmp;
		medianBlur(frame_hsv(sample_rects[i]), tmp, BOX_SIZE);
		int mid = BOX_SIZE / 2;
		sample_medians.push_back(Scalar(tmp.at<Vec3b>(mid, mid)));

	}

}

// HSV szín normalizálásához használt függvény
Scalar normalizeHSVScalar(const Scalar& scalar) {

	Scalar normalized(scalar);

	if (scalar[0] < 0) normalized[0] = 0;
	if (scalar[0] > 255) normalized[0] = 255;

	if (scalar[1] < 0) normalized[1] = 0;
	if (scalar[1] > 255) normalized[1] = 255;

	if (scalar[2] < 0) normalized[2] = 0;
	if (scalar[2] > 255) normalized[2] = 255;

	return normalized;

}

// maszk létrehozása a mintavételezési téglalapok mediánjai alapján
void getMaskByMedians() {

	// üres maszk
	mask = Mat::zeros(HEIGHT, WIDTH, CV_8UC1);

	// mediánok bejárása
	for (int i = 0; i < sample_medians.size(); i++) {

		// egy új maszk
		Mat mask_tmp = Mat::zeros(HEIGHT, WIDTH, CV_8UC1);

		inRange(frame_hsv, normalizeHSVScalar(sample_medians[i] - DOWN), normalizeHSVScalar(sample_medians[i] + UP), mask_tmp);

		mask += mask_tmp;

	}

}


// maszk létrehozása
void getMask() {

	getMaskByMedians();

	// 3x3 as kernel létrehozása
	int ksize = 3;
	Mat kernel(ksize, ksize, CV_8U);
	kernel.setTo(1);
	int mid = ksize / 2;
	kernel.at<uchar>(mid, mid) = 0;

	// erode(mask, mask, kernel, Point(-1, -1), 1);
	dilate(mask, mask, kernel, Point(-1, -1), 2);

}

// ujjpontok kirajzolása
void drawFingerPoints() {

	for (int i = 0; i < finger_points.size(); i++) {
		circle(frame, finger_points[i], 3, GREEN, 2);
	}

}

// két pont közötti távolság meghatározása
double pointDistance(Point point1, Point point2) {
	return norm(point1 - point2);
}

// három pont által meghatározott szög meghatározása (koszinusztétel implementációja)
float angle(Point base, Point point1, Point point2) {

	double a = pointDistance(base, point1);
	double b = pointDistance(base, point2);
	double c = pointDistance(point1, point2);

	double cosGamma = (a*a + b*b - c*c) / (2 * a*b);

	double ang = acos(cosGamma) * 180 / PI;

	return ang;

}

// a paraméterül kapott két Point vektor alapján annak meghatározása, hogy melyik pont hiányzik a második vektorból, ami az elsõben még szerepelt
int indexOfMissingPoint(const vector<Point>& full, const vector<Point>& missing) {

	// logikai tömb létrehozása, az egyes értékek jelzik, hogy az elsõ vektorban szereplõ pontok közül melyik található meg a másodikban
	bool* found = new bool[full.size()];
	for (int i = 0; i < full.size(); i++) {
		found[i] = false;
	}

	// pontok páronkénti bejárása
	// ha két pont eléggé közel van egymáshoz (egy elõre megadott érték alapján), akkor azt mondjuk, hogy benne van a második vektorban
	for (int i = 0; i < missing.size(); i++) {
		for (int j = 0; j < full.size(); j++) {

			if (pointDistance(missing[i], full[j]) < 20) {
				found[j] = true;
				break;
			}

		}
	}

	// amelyik pont nincs benne a második vektorban, annak az indexét visszaadjuk
	for (int i = 0; i < full.size(); i++) {
		if (!found[i]) {
			delete[] found;
			return i;
		}
	}

	// ha nincs ilyen pont, akkor -1 a visszatérési érték
	delete[] found;
	return -1;

}

// klikkelés eseményének megvalósítása
void performClick() {

	// a klikkelés helye az a pont lesz, amelyik ujjpontunk eltûnt egyik képkockáról a másikra lépve (behajlítottuk)
	int index = indexOfMissingPoint(fingertips_before, fingertips_actual);

	// pont átadása a számológépnek feldolgozásra
	calc.click(fingertips_before[index].x, fingertips_before[index].y);

}

// események észlelésére szolgáló függvény
void checkForEvents() {

	// ha az ujjak száma egyik képkockáról a másikra 5-rõl 4-re csökkent, az azt jelenti, hogy a felhasználó kattintott az egyik ujjával
	if (fingertips_before.size() == 5 && fingertips_actual.size() == 4) {
		performClick();
	}

	//ha a külsõ kör mérete a megadott aránnyal csökken, akkor azt úgy vesszük,
	//hogy a kéz össze lett csukva
	if (last_avg*(1.0 - decrease_percent) >= radius)
	{
		average_radius = 0.0;
		last_avg = 0;
		radius_samples.clear();
		cout << "oszecsuktad a kezedet" << endl;
		counter++;
	}

	//a kéz kétszeri összecsukása esetén a program végrehajtása befejezõdik
	if (counter == 2)
	{
		running = false;
		counter = 0;
	}

}

int main()
{

	namedWindow("frame");
	setMouseCallback("frame", on_mouse_click);

	videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, WIDTH);
	videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, HEIGHT);

	// mintavételezési pontok inicializálása
	initSamplePoints();

	// a mintavételezéshez használt ciklus
	while (true) {

		// kép beolvasása a webkameráról
		videoCapture >> frame;
		// kép tükrözése
		flip(frame, frame, 1);

		// áttérés HSV színtérbe
		cvtColor(frame, frame_hsv, COLOR_BGR2HSV);

		// mintavételezéshez használt téglalapok kirajzolása
		drawSampleRects(frame);

		//rectangle(frame, MAIN_RECT, Scalar(0, 0, 255));

		// kép megjelenítése
		imshow("frame", frame);

		char c = waitKey(1000 / FPS);
		// szõköz lenyomása esetén mintát veszünk és befejezzük a ciklust
		if (c == 32) break;
		if (c == 'd')
		{
			sample_mode = !sample_mode;
			if (sample_mode)
			{
				cout << "Sample mode 1" << endl;
			}
			else
			{
				cout << "Sample mode 2" << endl;
			}
		}

	}

	//Az  TODO ide kell :D
	// mediánok meghatározása a mintavételezésbõl nyert adatok alapján
	if (sample_mode)
	{
		getSampleMedians();
	}
	else
	{
		getSampleMedians_2();
	}


	// a számológép használata
	while (true) {

		// kép beolvasása a webkameráról
		videoCapture >> frame;
		// kép tükrözése
		flip(frame, frame, 1);

		// képkocka számláló változó növelése
		fps_counter++;

		// az alkalmazás bezárásához használt gesztus felismeréséhez van hozzá szükségünk
		// 4 másodperc elteltével nullázódik (így a gesztus nem lesz túl "hosszú")
		if (FPS * 4 <= fps_counter) {
			fps_counter = 0;
			counter = 0;
		}

		// egy kis zajszûrés
		Mat blurred;
		blur(frame, blurred, Size(5, 5));

		// áttérés HSV színtérbe
		cvtColor(blurred, frame_hsv, COLOR_BGR2HSV);

		// maszk elõállítása
		getMask();

		// az összes kontúr megtalálása
		vector<vector<Point>> contours;
		vector<Vec4i> hierarcy;
		findContours(mask(MAIN_RECT), contours, hierarcy, RETR_EXTERNAL, CHAIN_APPROX_NONE);

		// a kontúrok közül a legnagyobb területû megtalálása
		int biggestCountourIndex = -1;
		int biggestCountourSize = 0;
		for (int i = 0; i < contours.size(); i++) {
			if (contours[i].size() > biggestCountourSize) {
				biggestCountourIndex = i;
				biggestCountourSize = contours[i].size();

			}
		}

		// ha van legnagyobb méretû kontúr
		if (biggestCountourIndex != -1) {

			vector<int> hull;
			vector<Point> poly;
			Rect bound_rect;
			vector<Vec4i> defects;

			// kéz konvex burkának meghatározása
			convexHull(contours[biggestCountourIndex], hull, true);
			// approxPolyDP meghatározása
			approxPolyDP(contours[biggestCountourIndex], poly, 3, false);
			// kezet körbevevõ legkisebb téglalap meghatározása
			bound_rect = boundingRect(poly);
			// téglalap kirajzolása
			rectangle(frame, bound_rect, Scalar(0, 0, 255));

			vector<Point> new_poly;
			for each (Point p in poly)
			{
				if (bound_rect.y + bound_rect.height*0.75 >= p.y)
				{
					new_poly.push_back(p);
				}
			}

			// a kezet körbevevõ, minimális méretû kör meghatározása
			Point2f center;
			minEnclosingCircle((Mat)new_poly, center, radius);
			// kör kirajzolása
			circle(frame, center, radius, Scalar(0, 0, 255), 2);

			// az aktuális képkocán a kör sugarának mentése
			radius_samples.push_back(radius);
			// legutóbbi átlagos sugárméret mentése
			last_avg = average_radius;
			// átlagos sugárméret nullázása
			average_radius = 0.0;

			// az elõzõ 6 képkocka sugárméreteinek összeadása
			if (radius_samples.size() >= 6) {

				for (size_t i = 0; i < 6; i++) {
					average_radius += radius_samples[i];
				}

				// a legelsõ sugárméret kitörlése, így a vektor mérete sosem lesz 6-nál több
				radius_samples.erase(radius_samples.begin(), ++radius_samples.begin());

			}

			// átlag kiszámítása
			average_radius /= 6.0;


			//line(frame, Point(0, bound_rect.y + bound_rect.height*0.75), Point(WIDTH, bound_rect.y + bound_rect.height*0.75), Scalar(0, 0, 255));

			// a kezet körbevevõ konvex hurok kirajzolása
			for (int i = 0; i < hull.size() - 1; i++) {
				line(frame, contours[biggestCountourIndex][hull[i]], contours[biggestCountourIndex][hull[i + 1]], Scalar(0, 0, 255));
			}

			// konvexitási defektus pontok meghatározása
			convexityDefects(contours[biggestCountourIndex], hull, defects);
			// szûrt defektus pontok vektora
			vector<Vec4i> filtered_defects;

			// defektus pontok szûrése a defektus hossza és a pontok elhelyezkedése alapján
			for (int i = 0; i < defects.size(); i++) {

				// az adott defektus pont fontosabb adatainak lekérése
				int start_index = defects[i][0];
				int end_index = defects[i][1];
				int far_index = defects[i][2];


				if (contours[biggestCountourIndex][start_index].y >= bound_rect.y + bound_rect.height*0.75
					|| contours[biggestCountourIndex][end_index].y >= bound_rect.y + bound_rect.height*0.75
					|| contours[biggestCountourIndex][far_index].y >= bound_rect.y + bound_rect.height*0.75)
				{
					continue;
				}

				if (defects[i][3] < 20 * 256)
				{
					continue;
				}

				filtered_defects.push_back(defects[i]);

			}

			// korábbi ujjpontok törlése
			finger_points.clear();

			// ujjpontok meghatározása
			for (int i = 0; i < filtered_defects.size(); i++) {

				// az adott defektus pont fontosabb adatainak lekérése, meghatározása
				int start_index = filtered_defects[i][0];
				int end_index = filtered_defects[i][1];
				int far_index = filtered_defects[i][2];
				double distance = defects[i][3];
				double ang = angle(contours[biggestCountourIndex][far_index], contours[biggestCountourIndex][start_index], contours[biggestCountourIndex][end_index]);

				// a start_index és end_index indexekkel rendelkezõ kontúrpontok lehetnek a potenciális ujjpontok
				// ezért mind a kettõt hozzáadjuk az ujjpontok vektorához, feltéve ha az már nem szerepel korábban a vektorban
				// ezt nem szó szerint kell érteni
				// ha két pont egy bizonyos értéknél kisebb távolságra vannak egymástól, akkor úgy vesszük, hogy az a két pont megegyezik

				// start_index indexû kontrúrpont hozzáadása, ha még nem szerepelt eddig
				bool close = false;
				int min_distance = 30;
				for (int j = 0; j < finger_points.size(); j++) {
					if (pointDistance(finger_points[j], contours[biggestCountourIndex][start_index]) < min_distance) {
						close = true;
						break;
					}
				}
				if (!close) {
					finger_points.push_back(contours[biggestCountourIndex][start_index]);
				}

				// end_index indexû kontrúrpont hozzáadása, ha még nem szerepelt eddig
				close = false;
				for (int j = 0; j < finger_points.size(); j++) {
					if (pointDistance(finger_points[j], contours[biggestCountourIndex][end_index]) < min_distance) {
						close = true;
						break;
					}
				}
				if (!close) {
					finger_points.push_back(contours[biggestCountourIndex][end_index]);
				}

			}

			// ujjpontok kirajzolása
			drawFingerPoints();

		}

		// ujjpontok mentése késõbbi használatra (az eseménykezeléshez)
		fingertips_before = fingertips_actual;
		fingertips_actual = finger_points;

		// események kezelése
		checkForEvents();

		// program leállítása, ha szükséges
		if (!running) {
			return 0;
		}

		rectangle(frame, MAIN_RECT, Scalar(0, 0, 255));

		// számológép kirajzolása
		calc.show(frame);

		// webkamera képének megjelenítése
		imshow("frame", frame);

		// maszk kirajzolása
		imshow("mask", mask);

		// ESC billentyû lenyomása esetén a program bezárása
		if (waitKey(1000 / FPS) == 27) break;

	}

	return 0;
}

void on_mouse_click(int event, int x, int y, int, void*) {
	if (event != EVENT_LBUTTONDOWN)
		return;

	calc.click(x, y);

}
