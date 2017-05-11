#include "calculator.h"
#include <cmath>


calculator::calculator(double scale, int x, int y)
{
	showed = false;
	this->scale = scale;
	height = (int)(scale*baseWidth);
	width = (int)(scale*baseWidth);
	this->x = x;
	this->y = y;

	keys[0][0] = "0";
	keys[0][1] = "0";
	keys[0][2] = ",";
	keys[0][3] = "+";
	keys[0][4] = "=";

	keys[1][0] = "1";
	keys[1][1] = "2";
	keys[1][2] = "3";
	keys[1][3] = "-";
	keys[1][4] = "=";

	keys[2][0] = "4";
	keys[2][1] = "5";
	keys[2][2] = "6";
	keys[2][3] = "*";
	keys[2][4] = "1/x";

	keys[3][0] = "7";
	keys[3][1] = "8";
	keys[3][2] = "9";
	keys[3][3] = "/";
	keys[3][4] = "%";

	keys[4][0] = "<-";
	keys[4][1] = "CE";
	keys[4][2] = "C";
	keys[4][3] = "neg";
	keys[4][4] = "sqrt";
}


calculator::~calculator()
{
}

//tizedes vessz� jelz�s�re
bool isComma = false;
//jelenleg be�rt karakterek kinnul�z�s�nak jejz�s�re
bool isReset = false;
//egy operandus� m�velet jelz�s�re
bool isOperandA = false;
char operandA = '0';
//k�t operandus� m�velet jelz�s�re
bool isOperandB = false;
char operandB = '0';
//�rv�nytelen m�velet m�velet jelz�s�re
bool isNaN = false;


vector<string> calculations;
string current = "0";
string strA = "";
double valueA = 0;
double valueB = 0;

//klikkel koordin�t�k t�rol�sa
int beforeX = -1;
int beforeY = -1;


//sz�mol�g�p kirajzol�s�t v�gz� f�gv�ny
void
calculator::show(Mat& dst) {
	showed = true;
	rectangle(dst, Point(x, y), Point(x + width, y + height), Scalar(255, 0, 0), 2);

	int stepX = (int)(width / 5.0);
	int stepY = (int)(height / 7.0);

	//hat�rvonalak rajzol�sa
	for (int i = 1; i <= 5; ++i) {
		int minus = i*stepY;
		if (i != 1)
			line(dst, Point(x, y + height - minus), Point(x + width, y + height - minus), Scalar(255, 0, 0), 2);
		else
			line(dst, Point(x, y + height - minus), Point(x + width - stepX, y + height - minus), Scalar(255, 0, 0), 2);
	}

	for (int i = 1; i <= 4; ++i) {
		int plus = i*stepX;
		if (i != 1)
			line(dst, Point(x + plus, y + 2 * stepY), Point(x + plus, y + height), Scalar(255, 0, 0), 2);
		else
			line(dst, Point(x + plus, y + 2 * stepY), Point(x + plus, y + height - stepY), Scalar(255, 0, 0), 2);
	}

	//"0" jel kirajzol�sa
	if ((beforeX == 0 || beforeX == 1) && beforeY == 0)
	{
		putText(dst, "0", Point((int)(x + 1 * stepX - scale * 10), (int)(y + height - 8)), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 1.3, Scalar(0, 255, 0), 2);
		beforeX = -1;
		beforeY = -1;
	}
	else
	{
		putText(dst, "0", Point((int)(x + 1 * stepX - scale * 10), (int)(y + height - 8)), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 1.3, Scalar(0, 0, 255), 2);
	}

	//"=" jel kirajzol�sa
	if ((beforeY == 0 || beforeY == 1) && beforeX == 4)
	{
		putText(dst, "=", Point((int)(x + 4 * stepX + stepX / 2 - scale * 10), (int)(y - stepY / 2 + height - 8)), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 1.3, Scalar(0, 255, 0), 2);
		beforeX = -1;
		beforeY = -1;
	}
	else
	{
		putText(dst, "=", Point((int)(x + 4 * stepX + stepX / 2 - scale * 10), (int)(y - stepY / 2 + height - 8)), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 1.3, Scalar(0, 0, 255), 2);
	}


	//t�bbi karakter kirajzol�sa
	for (int i = 0; i < 5; ++i) {
		for (int j = 0; j < 5; j++) {
			if (keys[i][j].compare("=") == 0 || keys[i][j].compare("0") == 0)
			{
				continue;
			}
			Scalar color(0, 0, 255);
			if (j == beforeX && i == beforeY)
			{
				color = Scalar(0, 255, 0);
				beforeX = -1;
				beforeY = -1;
			}
			putText(dst, keys[i][j], Point((int)(x + j * stepX + stepX / 2 - scale * 10), (int)(y + height - i * stepY - 8/* * scale*keys[i][j].size()*/)), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * (1.0 / keys[i][j].size() + 0.3), color, 2);

		}
	}

	//a jelenleg be�rt karakterek hossz�nak figyel�se �s t�rdel�se ha sz�ks�ges, illetve a megjelen�t�se
	int length = current.length();
	string current_cpy = current;
	if (length > 16)
	{
		current_cpy.erase(0, length - 16);
		current_cpy = "<<" + current_cpy;
	}
	length = current_cpy.length();
	putText(dst, current_cpy, Point(x + width - length * 13 * scale - 20, y + height - 5 * stepY - 10), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 1.0, Scalar(0, 0, 255), 2);

	//az eddig v�grehajtott m�veletek karaktereinek hossz�nak figyel�se �s t�rdel�se ha sz�ks�ges, illetve a megjelen�t�se
	string calc_way = "";
	for (int i = 0; i < calculations.size(); i++)
	{
		calc_way += calculations[i];
		if (i != calculations.size() - 1)
		{
			calc_way += " ";
		}
	}
	length = calc_way.length();
	if (length > 23)
	{
		calc_way.erase(0, length - 23);
		calc_way = "<<" + calc_way;
	}
	length = calc_way.length();
	putText(dst, calc_way, Point(x + width - length * 9 * scale - 20, y + height - 6 * stepY - 10), CV_FONT_HERSHEY_COMPLEX_SMALL, scale * 0.7, Scalar(0, 0, 255), 2);
}


//a tizedes vessz� ut�ni f�l�sleges null�k elhagy�sa
void deleteZeros() {
	if (isComma)
	{
		while (current.at(current.length() - 1) == '0' || current.at(current.length() - 1) == '.')
		{
			if (current.length() != 1)
			{
				current.erase(current.length() - 1, 1);
			}
			else
			{
				break;
			}
		}
	}
}

//egyoperandus� m�velet v�grehajt�sa
void calculateA() {

	isReset = true;

	deleteZeros();
	stringstream ss;
	ss << current;
	ss >> valueB;

	isOperandA = true;
	isOperandB = false;

	switch (operandA)
	{
		//neg�ci�
	case 'n':
		valueB *= -1;
		if (strA.empty())
		{
			strA = "neg(" + current + ")";
			calculations.push_back(strA);
		}
		else
		{
			strA = "neg(" + strA + ")";
			calculations[calculations.size() - 1] = strA;
		}
		break;
		//sqrt
	case 's':
		if (valueB < 0)
		{
			isNaN = true;
			current = "NaN";
			return;
		}
		valueB = sqrt(valueB);
		if (strA.empty())
		{
			strA = "sqrt(" + current + ")";
			calculations.push_back(strA);
		}
		else
		{
			strA = "sqrt(" + strA + ")";
			calculations[calculations.size() - 1] = strA;
		}
		break;
		//reciprok
	case '1':
		if (valueB == 0.0)
		{
			isNaN = true;
			current = "NaN";
			return;
		}
		valueB = 1.0 / valueB;
		if (strA.empty())
		{
			strA = "rec(" + current + ")";
			calculations.push_back(strA);
		}
		else
		{
			strA = "rec(" + strA + ")";
			calculations[calculations.size() - 1] = strA;
		}
		break;
		//sz�zal�k sz�m�t�sa
	case '%':
		valueB = valueA*valueB / 100.0;
		break;
	}


	ss.str("");
	ss.clear();
	ss << valueB;
	current = ss.str();
	if (operandA == '%')
	{
		calculations.push_back(current);
	}
}


//k�toperandus� m�velet v�grehajt�sa
void calculateB() {
	isReset = true;


	if (!isOperandB)
	{
		deleteZeros();
		stringstream ss;
		ss << current;
		ss >> valueB;


		if (isOperandA)
		{
			//calculations.push_back(strA);
			strA = "";
			isOperandA = false;
		}
		else
		{
			calculations.push_back(current);
		}

		switch (operandB)
		{
			//�sszead�s
		case '+':
			valueA += valueB;
			break;
			//kivon�s
		case '-':
			valueA -= valueB;
			break;
			//oszt�s
		case '/':
			if (valueB == 0.0)
			{
				isNaN = true;
				current = "NaN";
				return;
			}
			valueA /= valueB;
			break;
			//szorz�s
		case '*':
			valueA *= valueB;
			break;
			//els� m�velet eset�n
		default:
			valueA = valueB;
			break;
		}

		isOperandB = true;
		ss.str("");
		ss.clear();
		ss << valueA;
		current = ss.str();
	}
	else
	{
		if (calculations.size() != 0)
			calculations.pop_back();
	}

}

//onclik esem�ny lekezel�se
void
calculator::click(int _x, int _y) {

	if (!showed)
	{
		return;
	}

	//sz�mol�g�p eredeti koordin�t�inak visszanyer�se
	int stepX = (int)(width / 5.0);
	int stepY = (int)(height / 7.0);
	_x -= x;
	_x = (int)((double)_x / (double)stepX);
	_y -= y;
	_y = (int)(((double)height - (double)_y) / (double)stepY);

	//t�lindexel�s eset�n kil�p�s
	if (_x > 4 || _y > 4 || _x < 0 || _y < 0)
	{
		return;
	}

	beforeX = _x;
	beforeY = _y;

	//sz�mok �s tizedes vessz� lekezel�se
	if (_x <= 2 && _y <= 3 && !isNaN)
	{
		isOperandB = false;
		if (isOperandA)
		{
			calculations.pop_back();
			isOperandA = false;
			strA = "";
		}
		if (isReset)
		{
			current = "0";
			isReset = false;
		}
		switch (keys[_y][_x].at(0))
		{
		case ',':
			if (!isComma)
			{
				isComma = true;
				current += ".";
			}
			break;
		case '0':
			if (isComma)
			{
				current += "0";
			}
			else if (current.at(current.length() - 1) != '0')
			{
				current += "0";
			}
			break;
		default:
			if (current.length() == 1 && current.at(0) == '0')
			{
				current = keys[_y][_x];
			}
			else
			{
				current += keys[_y][_x];
			}
			break;
		}
	}

	//t�rl� m�veletek lekezel�se
	if (_x <= 2 && _y == 4)
	{
		switch (keys[_y][_x].at(keys[_y][_x].length() - 1))
		{
		case '-':
			if (!isReset)
			{
				if (current.at(current.length() - 1) == '.')
				{
					isComma = false;
				}

				current.erase(current.length() - 1, 1);
				if (current.length() == 0)
				{
					current = "0";
				}
				if (current.length() == 1)
				{
					if (current.at(0) == '-')
					{
						current = "0";
					}
				}
			}
			else
			{
				current = "0";
			}
			break;
		case 'C':
			current = "0";
			calculations.clear();
			isComma = false;
			isNaN = false;
			isOperandA = false;
			isOperandB = false;
			isReset = false;
			operandA = '0';
			strA = "";
			valueA = 0;
			valueB = 0;
			break;
		case 'E':
			current = "0";
			isComma = false;
			if (isOperandA)
			{
				calculations.pop_back();
				isOperandA = false;
			}
			strA = "";
			if (isNaN)
			{
				calculations.clear();
				isNaN = false;
			}
			valueB = 0;
			break;
		}
	}

	//sz�mol�si m�veletek lekezel�se
	if (_x >= 3 && !isNaN)
	{
		switch (keys[_y][_x].at(0))
		{
		case 'n':
		case 's':
		case '%':
		case '1':
			operandA = keys[_y][_x].at(0);
			calculateA();
			break;
		case '+':
			calculateB();
			operandB = '+';
			calculations.push_back("+");
			break;
		case '-':
			calculateB();
			operandB = '-';
			calculations.push_back("-");
			break;
		case '*':
			calculateB();
			operandB = '*';
			calculations.push_back("*");
			break;
		case '/':
			calculateB();
			operandB = '/';
			calculations.push_back("/");
			break;
			//egyenl�s�g gomb megnyom�sa ut�n az utols� m�velet v�rehajt�sa �s a sz�ks�ges v�ltoz�k be�ll�t�sa
		default:
			isOperandB = false;
			calculateB();
			isOperandB = false;
			operandB = '0';
			isComma = false;
			isNaN = false;
			isOperandA = false;
			isReset = true;
			operandA = '0';
			strA = "";
			//valueB = 0;
			calculations.clear();
			break;
		}
	}

	/*
	cout << "valueA " << valueA << endl;
	cout << "valueB " << valueB << endl;
	cout << "current " << current << endl;

	cout << "calculations ";
	for (size_t i = 0; i < calculations.size(); i++)
	{
	cout << calculations[i] << " ";
	}
	cout << endl;
	*/

}