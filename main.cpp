#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <cmath>

#include "calculator.h"

using namespace cv;
using namespace std;

// �lland� �rt�kek
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

// sz�nek
const Scalar GREEN(0, 255, 0);

// webkamera
VideoCapture videoCapture(0);

// k�pek
Mat frame;
Mat frame_hsv;
Mat mask;

// a k�z sz�n�nek mintav�tlez�s�hez haszn�lt v�ltoz�k
vector<Point> sample_points;
vector<Rect> sample_rects;
vector<Scalar> sample_medians;
bool sample_mode = true;

// a k�z meghat�roz�s�hoz �s esem�nyek kezel�s�hez haszn�lt v�ltoz�k
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

// sz�mol�g�p
calculator calc(1.2, 60, 25);

void on_mouse_click(int, int, int, int, void*);

// a k�z sz�n�nek mintav�telez�s�hez haszn�lt v�ltoz�k inicializ�l�sa
void initSamplePoints() {

	// mintav�zelez�si t�glalapok bal fels� sarkait tartalmaz� vektor ki�r�t�se
	sample_points.clear();

	// mintav�telez�si t�glalapok bal fels� sark�nak inicializ�l�sa
	sample_points.push_back(Point(WIDTH / 2 - TRANSX, HEIGHT / 2 - 50 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - 25 - TRANSX, HEIGHT / 2 - 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 + 25 - TRANSX, HEIGHT / 2 - 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - TRANSX, HEIGHT / 2 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 - 25 - TRANSX, HEIGHT / 2 + 25 - TRANSY));
	sample_points.push_back(Point(WIDTH / 2 + 25 - TRANSX, HEIGHT / 2 + 25 - TRANSY));

	// mintav�telez�si t�glalapok l�trehoz�sas
	for (int i = 0; i < sample_points.size(); i++) {
		sample_rects.push_back(Rect(sample_points[i], Size(BOX_SIZE, BOX_SIZE)));
	}

}

// a mintav�telez�shez haszn�lt t�glalapok kirajzol�sa a c�l k�pre
void drawSampleRects(Mat& dest) {

	for (int i = 0; i < sample_rects.size(); i++) {
		rectangle(dest, sample_rects[i], GREEN);
	}

}

// uchar t�pus� vektor elemeinek medi�nj�nak meghat�roz�sa
uchar median(vector<uchar>& values) {

	// elemek n�vekv� sorrendbe rendez�se
	sort(values.begin(), values.end());

	// p�ros darabsz�m� vektor eset�n a k�t k�z�ps� elem �tlaga a medi�n
	if (values.size() % 2 == 0) {
		return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2;
	}
	// p�ratlan darabsz�m� vektor eset�n a k�z�ps� elem a medi�n
	else {
		return values[values.size() / 2];
	}

}

// k�p sz�neinek medi�nj�nak meghat�roz�sa
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

// a mintav�telez�si t�glalapokb�l a medi�nok kinyer�se
void getSampleMedians() {

	for (int i = 0; i < sample_rects.size(); i++) {

		// a mintav�telez�si t�glalapb�l kihagyjuk a z�ld keretet, ehhez �j jobb fels� �s bal als� sarkak kiv�laszt�sa
		Point top_left = sample_rects[i].tl() + Point(1, 1);
		Point bottom_right = sample_rects[i].br() - Point(1, 1);

		// medianBlur sz�r� alkalmaz�sa a sz�ks�ges r�szre
		Mat area = frame_hsv(Rect(top_left, bottom_right));
		// medianBlur(frame_hsv(Rect(top_left, bottom_right)), area, BOX_SIZE);

		// medi�n hozz�f�z�se a list�hoz
		sample_medians.push_back(getMedianOfMat(area));

	}

}

// a mintav�telez�si t�glalapokb�l a medi�nok kinyer�se
void getSampleMedians_2() {

	for (int i = 0; i < sample_rects.size(); i++) {

		Mat tmp;
		medianBlur(frame_hsv(sample_rects[i]), tmp, BOX_SIZE);
		int mid = BOX_SIZE / 2;
		sample_medians.push_back(Scalar(tmp.at<Vec3b>(mid, mid)));

	}

}

// HSV sz�n normaliz�l�s�hoz haszn�lt f�ggv�ny
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

// maszk l�trehoz�sa a mintav�telez�si t�glalapok medi�njai alapj�n
void getMaskByMedians() {

	// �res maszk
	mask = Mat::zeros(HEIGHT, WIDTH, CV_8UC1);

	// medi�nok bej�r�sa
	for (int i = 0; i < sample_medians.size(); i++) {

		// egy �j maszk
		Mat mask_tmp = Mat::zeros(HEIGHT, WIDTH, CV_8UC1);

		inRange(frame_hsv, normalizeHSVScalar(sample_medians[i] - DOWN), normalizeHSVScalar(sample_medians[i] + UP), mask_tmp);

		mask += mask_tmp;

	}

}


// maszk l�trehoz�sa
void getMask() {

	getMaskByMedians();

	// 3x3 as kernel l�trehoz�sa
	int ksize = 3;
	Mat kernel(ksize, ksize, CV_8U);
	kernel.setTo(1);
	int mid = ksize / 2;
	kernel.at<uchar>(mid, mid) = 0;

	// erode(mask, mask, kernel, Point(-1, -1), 1);
	dilate(mask, mask, kernel, Point(-1, -1), 2);

}

// ujjpontok kirajzol�sa
void drawFingerPoints() {

	for (int i = 0; i < finger_points.size(); i++) {
		circle(frame, finger_points[i], 3, GREEN, 2);
	}

}

// k�t pont k�z�tti t�vols�g meghat�roz�sa
double pointDistance(Point point1, Point point2) {
	return norm(point1 - point2);
}

// h�rom pont �ltal meghat�rozott sz�g meghat�roz�sa (koszinuszt�tel implement�ci�ja)
float angle(Point base, Point point1, Point point2) {

	double a = pointDistance(base, point1);
	double b = pointDistance(base, point2);
	double c = pointDistance(point1, point2);

	double cosGamma = (a*a + b*b - c*c) / (2 * a*b);

	double ang = acos(cosGamma) * 180 / PI;

	return ang;

}

// a param�ter�l kapott k�t Point vektor alapj�n annak meghat�roz�sa, hogy melyik pont hi�nyzik a m�sodik vektorb�l, ami az els�ben m�g szerepelt
int indexOfMissingPoint(const vector<Point>& full, const vector<Point>& missing) {

	// logikai t�mb l�trehoz�sa, az egyes �rt�kek jelzik, hogy az els� vektorban szerepl� pontok k�z�l melyik tal�lhat� meg a m�sodikban
	bool* found = new bool[full.size()];
	for (int i = 0; i < full.size(); i++) {
		found[i] = false;
	}

	// pontok p�ronk�nti bej�r�sa
	// ha k�t pont el�gg� k�zel van egym�shoz (egy el�re megadott �rt�k alapj�n), akkor azt mondjuk, hogy benne van a m�sodik vektorban
	for (int i = 0; i < missing.size(); i++) {
		for (int j = 0; j < full.size(); j++) {

			if (pointDistance(missing[i], full[j]) < 20) {
				found[j] = true;
				break;
			}

		}
	}

	// amelyik pont nincs benne a m�sodik vektorban, annak az index�t visszaadjuk
	for (int i = 0; i < full.size(); i++) {
		if (!found[i]) {
			delete[] found;
			return i;
		}
	}

	// ha nincs ilyen pont, akkor -1 a visszat�r�si �rt�k
	delete[] found;
	return -1;

}

// klikkel�s esem�ny�nek megval�s�t�sa
void performClick() {

	// a klikkel�s helye az a pont lesz, amelyik ujjpontunk elt�nt egyik k�pkock�r�l a m�sikra l�pve (behajl�tottuk)
	int index = indexOfMissingPoint(fingertips_before, fingertips_actual);

	// pont �tad�sa a sz�mol�g�pnek feldolgoz�sra
	calc.click(fingertips_before[index].x, fingertips_before[index].y);

}

// esem�nyek �szlel�s�re szolg�l� f�ggv�ny
void checkForEvents() {

	// ha az ujjak sz�ma egyik k�pkock�r�l a m�sikra 5-r�l 4-re cs�kkent, az azt jelenti, hogy a felhaszn�l� kattintott az egyik ujj�val
	if (fingertips_before.size() == 5 && fingertips_actual.size() == 4) {
		performClick();
	}

	//ha a k�ls� k�r m�rete a megadott ar�nnyal cs�kken, akkor azt �gy vessz�k,
	//hogy a k�z �ssze lett csukva
	if (last_avg*(1.0 - decrease_percent) >= radius)
	{
		average_radius = 0.0;
		last_avg = 0;
		radius_samples.clear();
		cout << "oszecsuktad a kezedet" << endl;
		counter++;
	}

	//a k�z k�tszeri �sszecsuk�sa eset�n a program v�grehajt�sa befejez�dik
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

	// mintav�telez�si pontok inicializ�l�sa
	initSamplePoints();

	// a mintav�telez�shez haszn�lt ciklus
	while (true) {

		// k�p beolvas�sa a webkamer�r�l
		videoCapture >> frame;
		// k�p t�kr�z�se
		flip(frame, frame, 1);

		// �tt�r�s HSV sz�nt�rbe
		cvtColor(frame, frame_hsv, COLOR_BGR2HSV);

		// mintav�telez�shez haszn�lt t�glalapok kirajzol�sa
		drawSampleRects(frame);

		//rectangle(frame, MAIN_RECT, Scalar(0, 0, 255));

		// k�p megjelen�t�se
		imshow("frame", frame);

		char c = waitKey(1000 / FPS);
		// sz�k�z lenyom�sa eset�n mint�t vesz�nk �s befejezz�k a ciklust
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
	// medi�nok meghat�roz�sa a mintav�telez�sb�l nyert adatok alapj�n
	if (sample_mode)
	{
		getSampleMedians();
	}
	else
	{
		getSampleMedians_2();
	}


	// a sz�mol�g�p haszn�lata
	while (true) {

		// k�p beolvas�sa a webkamer�r�l
		videoCapture >> frame;
		// k�p t�kr�z�se
		flip(frame, frame, 1);

		// k�pkocka sz�ml�l� v�ltoz� n�vel�se
		fps_counter++;

		// az alkalmaz�s bez�r�s�hoz haszn�lt gesztus felismer�s�hez van hozz� sz�ks�g�nk
		// 4 m�sodperc eltelt�vel null�z�dik (�gy a gesztus nem lesz t�l "hossz�")
		if (FPS * 4 <= fps_counter) {
			fps_counter = 0;
			counter = 0;
		}

		// egy kis zajsz�r�s
		Mat blurred;
		blur(frame, blurred, Size(5, 5));

		// �tt�r�s HSV sz�nt�rbe
		cvtColor(blurred, frame_hsv, COLOR_BGR2HSV);

		// maszk el��ll�t�sa
		getMask();

		// az �sszes kont�r megtal�l�sa
		vector<vector<Point>> contours;
		vector<Vec4i> hierarcy;
		findContours(mask(MAIN_RECT), contours, hierarcy, RETR_EXTERNAL, CHAIN_APPROX_NONE);

		// a kont�rok k�z�l a legnagyobb ter�let� megtal�l�sa
		int biggestCountourIndex = -1;
		int biggestCountourSize = 0;
		for (int i = 0; i < contours.size(); i++) {
			if (contours[i].size() > biggestCountourSize) {
				biggestCountourIndex = i;
				biggestCountourSize = contours[i].size();

			}
		}

		// ha van legnagyobb m�ret� kont�r
		if (biggestCountourIndex != -1) {

			vector<int> hull;
			vector<Point> poly;
			Rect bound_rect;
			vector<Vec4i> defects;

			// k�z konvex burk�nak meghat�roz�sa
			convexHull(contours[biggestCountourIndex], hull, true);
			// approxPolyDP meghat�roz�sa
			approxPolyDP(contours[biggestCountourIndex], poly, 3, false);
			// kezet k�rbevev� legkisebb t�glalap meghat�roz�sa
			bound_rect = boundingRect(poly);
			// t�glalap kirajzol�sa
			rectangle(frame, bound_rect, Scalar(0, 0, 255));

			vector<Point> new_poly;
			for each (Point p in poly)
			{
				if (bound_rect.y + bound_rect.height*0.75 >= p.y)
				{
					new_poly.push_back(p);
				}
			}

			// a kezet k�rbevev�, minim�lis m�ret� k�r meghat�roz�sa
			Point2f center;
			minEnclosingCircle((Mat)new_poly, center, radius);
			// k�r kirajzol�sa
			circle(frame, center, radius, Scalar(0, 0, 255), 2);

			// az aktu�lis k�pkoc�n a k�r sugar�nak ment�se
			radius_samples.push_back(radius);
			// legut�bbi �tlagos sug�rm�ret ment�se
			last_avg = average_radius;
			// �tlagos sug�rm�ret null�z�sa
			average_radius = 0.0;

			// az el�z� 6 k�pkocka sug�rm�reteinek �sszead�sa
			if (radius_samples.size() >= 6) {

				for (size_t i = 0; i < 6; i++) {
					average_radius += radius_samples[i];
				}

				// a legels� sug�rm�ret kit�rl�se, �gy a vektor m�rete sosem lesz 6-n�l t�bb
				radius_samples.erase(radius_samples.begin(), ++radius_samples.begin());

			}

			// �tlag kisz�m�t�sa
			average_radius /= 6.0;


			//line(frame, Point(0, bound_rect.y + bound_rect.height*0.75), Point(WIDTH, bound_rect.y + bound_rect.height*0.75), Scalar(0, 0, 255));

			// a kezet k�rbevev� konvex hurok kirajzol�sa
			for (int i = 0; i < hull.size() - 1; i++) {
				line(frame, contours[biggestCountourIndex][hull[i]], contours[biggestCountourIndex][hull[i + 1]], Scalar(0, 0, 255));
			}

			// konvexit�si defektus pontok meghat�roz�sa
			convexityDefects(contours[biggestCountourIndex], hull, defects);
			// sz�rt defektus pontok vektora
			vector<Vec4i> filtered_defects;

			// defektus pontok sz�r�se a defektus hossza �s a pontok elhelyezked�se alapj�n
			for (int i = 0; i < defects.size(); i++) {

				// az adott defektus pont fontosabb adatainak lek�r�se
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

			// kor�bbi ujjpontok t�rl�se
			finger_points.clear();

			// ujjpontok meghat�roz�sa
			for (int i = 0; i < filtered_defects.size(); i++) {

				// az adott defektus pont fontosabb adatainak lek�r�se, meghat�roz�sa
				int start_index = filtered_defects[i][0];
				int end_index = filtered_defects[i][1];
				int far_index = filtered_defects[i][2];
				double distance = defects[i][3];
				double ang = angle(contours[biggestCountourIndex][far_index], contours[biggestCountourIndex][start_index], contours[biggestCountourIndex][end_index]);

				// a start_index �s end_index indexekkel rendelkez� kont�rpontok lehetnek a potenci�lis ujjpontok
				// ez�rt mind a kett�t hozz�adjuk az ujjpontok vektor�hoz, felt�ve ha az m�r nem szerepel kor�bban a vektorban
				// ezt nem sz� szerint kell �rteni
				// ha k�t pont egy bizonyos �rt�kn�l kisebb t�vols�gra vannak egym�st�l, akkor �gy vessz�k, hogy az a k�t pont megegyezik

				// start_index index� kontr�rpont hozz�ad�sa, ha m�g nem szerepelt eddig
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

				// end_index index� kontr�rpont hozz�ad�sa, ha m�g nem szerepelt eddig
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

			// ujjpontok kirajzol�sa
			drawFingerPoints();

		}

		// ujjpontok ment�se k�s�bbi haszn�latra (az esem�nykezel�shez)
		fingertips_before = fingertips_actual;
		fingertips_actual = finger_points;

		// esem�nyek kezel�se
		checkForEvents();

		// program le�ll�t�sa, ha sz�ks�ges
		if (!running) {
			return 0;
		}

		rectangle(frame, MAIN_RECT, Scalar(0, 0, 255));

		// sz�mol�g�p kirajzol�sa
		calc.show(frame);

		// webkamera k�p�nek megjelen�t�se
		imshow("frame", frame);

		// maszk kirajzol�sa
		imshow("mask", mask);

		// ESC billenty� lenyom�sa eset�n a program bez�r�sa
		if (waitKey(1000 / FPS) == 27) break;

	}

	return 0;
}

void on_mouse_click(int event, int x, int y, int, void*) {
	if (event != EVENT_LBUTTONDOWN)
		return;

	calc.click(x, y);

}
