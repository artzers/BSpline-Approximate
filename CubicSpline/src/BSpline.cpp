#include <Eigen/Dense> //Eigen
#include <iostream>
#include "..\include\BSpline.h"


using namespace std;
using namespace Eigen;
/*!
 *\brief ����B���������˳�ƽ�
*\ param const std::vector<Point> & vecFitPoints ��ֵ��
*\ param double alpha ��˳��Ȩ��
*\ param double beta  �ƽ���Ȩ��
*\ Returns:   BSpline �ƽ����
*/
BSpline BSpline::CubicApproximate(const std::vector<Point>& vecFitPoints, double alpha, double beta)
{
	const int p = 3;
	BSpline bs;
	int  x = vecFitPoints.size();
	if (x < p)
	{
		cout << "too less point !" << endl;
		return bs;
	}
	//��Ҫ�ľ���
	Eigen::MatrixXd W = Eigen::MatrixXd::Zero(x + 2, x + 2);
	Eigen::MatrixXd P = Eigen::MatrixXd::Zero(x + 2, 3);
	Eigen::MatrixXd M = Eigen::MatrixXd::Zero(x + 2, x + 2);
	Eigen::MatrixXd F = Eigen::MatrixXd::Zero(x + 2, 3);
	//������
	bs.m_nDegree = p;
	bs.m_vecKnots.resize(x); //x+6���ڵ�
	//����ڵ�
	bs.m_vecKnots[0] = 0.0;
	for (int i = 1; i < x; ++i)
	{
		bs.m_vecKnots[i] = bs.m_vecKnots[i - 1]
			+ sqrt(PointDistance(vecFitPoints[i], vecFitPoints[i - 1]));
	}
	//�ڵ���β����p+1���ظ�
	bs.m_vecKnots.insert(bs.m_vecKnots.begin(), p, bs.m_vecKnots.front());
	bs.m_vecKnots.insert(bs.m_vecKnots.end(), p, bs.m_vecKnots.back());

	//W ����
	WMatrix(W, x + 2, p, bs.m_vecKnots);

	//M����
	std::vector<double> basis_func;
	M(0, 0) = 1;
	M(x - 1, x + 1) = 1;
	for (int i = p + 1; i < x + p - 1; ++i)
	{
		//c(u)�� N_{i-p},...,N_i��p+1���������Ϸ���
		bs.BasisFunc(bs.m_vecKnots[i], i, basis_func);
		for (int j = i - p, k = 0; j <= i; ++j, ++k)
		{
			M(i - p, j) = basis_func[k];
		}
	}
	//����
	M(x, 0) = -1;
	M(x, 1) = 1;
	M(x + 1, x) = -1;
	M(x + 1, x + 1) = 1;

	//F����
	for (int i = 0; i < x; ++i)
	{
		F(i, 0) = vecFitPoints[i](0);
		F(i, 1) = vecFitPoints[i](1);
		F(i, 2) = vecFitPoints[i](2);
	}

	{
		Vec3d v0, v1, v2;
		BesselTanget(vecFitPoints[0], vecFitPoints[1], vecFitPoints[2], v0, v1, v2);
		Vec3d v = v0 * (bs.m_vecKnots[p + 1] - bs.m_vecKnots[1]) / (double)p;
		F(x, 0) = v(0);
		F(x, 1) = v(1);
		F(x, 2) = v(2);
	}

	{
		Vec3d v0, v1, v2;
		BesselTanget(vecFitPoints[x - 3], vecFitPoints[x - 2], vecFitPoints[x - 1], v0, v1, v2);
		Vec3d v = v2 * (bs.m_vecKnots[x + 1 + p] - bs.m_vecKnots[x + 1]) / (double)p;
		F(x + 1, 0) = v(0);
		F(x + 1, 1) = v(1);
		F(x + 1, 2) = v(2);
	}

	//�ⷽ��
	P = (alpha*W + beta * M.transpose()*M).colPivHouseholderQr().solve(beta*M.transpose()*F);

#ifdef _DEBUG
	cout << "P------------------" << endl << P << endl;
	cout << "W------------------" << endl << W << endl;
	cout << "M------------------" << endl << M << endl;
	cout << "F------------------" << endl << F << endl;
#endif

	//�� P��ֵ���� B����
	bs.m_vecCVs.resize(x + 2);
	for (int i = 0; i < x + 2; ++i)
	{
		Point& cv = bs.m_vecCVs[i];
		cv(0) = P(i, 0);
		cv(1) = P(i, 1);
		cv(2) = P(i, 2);
	}

	return bs;
}

void BSpline::WMatrix(Eigen::MatrixXd& W, int n, int p, const std::vector<double>& u)
{
	std::vector<std::vector<std::pair<double, double>>> BasisFuncByKnot(n);
	//��ʼ��
	for (int i = 0; i < n; ++i)
	{
		BasisFuncByKnot[i].resize(n + p);
		for (int j = 0; j < n + p; ++j)
		{
			BasisFuncByKnot[i][j].first = 0;
			BasisFuncByKnot[i][j].second = 0;
		}
	}

	std::vector<std::pair<double, double>> a_b_array;
	for (int i = 0; i < n; ++i)
	{
		_SecondDerivativeCoefficient(i, u, a_b_array);
		std::copy(a_b_array.begin(), a_b_array.end(), BasisFuncByKnot[i].begin() + i);
	}
	//������//��ʵ����һ���Գƾ�����Ϊ�˷��㣬������һ��
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			double ret = 0;
			//����
			for (int k = 0; k < n + p; ++k)
			{
				const std::pair<double, double> basis_i = BasisFuncByKnot[i][k];
				const std::pair<double, double> basis_j = BasisFuncByKnot[j][k];

				ret += PolynomialIntegral(basis_i.first * basis_j.first, basis_i.first*basis_j.second +
					basis_i.second*basis_j.first, basis_i.second*basis_j.second, u[k], u[k + 1]);
			}
			W(i, j) = ret;
		}
	}
}

void BSpline::_SecondDerivativeCoefficient(int i, const std::vector<double>& u, std::vector<std::pair<double, double>>& a_b_array)
{
	const int p = 3;
	double c1, c2, c3;
	c1 = c2 = c3 = 0;

	double div = (u[i + p - 1] - u[i])*(u[i + p] - u[i]);
	if (div != 0)c1 = p * (p - 1) / div;

	div = (u[i + p] - u[i])*(u[i + p] - u[i + 1]);
	if (div != 0)c2 -= p * (p - 1) / div;
	div = (u[i + p] - u[i + 1])*(u[i + p + 1] - u[i + 1]);
	if (div != 0)c2 -= p * (p - 1) / div;

	div = (u[i + p + 1] - u[i + 1])*(u[i + p + 1] - u[i + 2]);
	if (div != 0)c3 = p * (p - 1) / div;

	a_b_array.resize(p + 1);
	for (int i = 0; i < p + 1; ++i)
	{
		a_b_array[i].first = 0;
		a_b_array[i].second = 0;
	}

	div = u[i + p - 2] - u[i];
	if (c1 != 0 && div != 0)
	{
		a_b_array[0].first = c1 / div;
		a_b_array[0].second = -c1 * u[i] / div;
	}

	div = u[i + p - 1] - u[i + 1];
	if (div != 0)
	{
		a_b_array[1].first = (c2 - c1) / div;
		a_b_array[1].second = (c1*u[i + p - 1] - c2 * u[i + 1]) / div;
	}

	div = u[i + p] - u[i + 2];
	if (div != 0)
	{
		a_b_array[2].first = (c3 - c2) / div;
		a_b_array[2].second = (c2*u[i + p] - c3 * u[i + 2]) / div;
	}

	div = u[i + p + 1] - u[i + 3];
	if (c3 != 0 && div != 0)
	{
		a_b_array[3].first = -c3 / div;
		a_b_array[3].second = u[i + p + 1] * c3 / div;
	}
}

double BSpline::PolynomialIntegral(double quad, double linear, double con, double start, double end)
{
	if (end == start)
		return 0;

	double ret = 0;
	if (quad != 0)
	{
		ret += (end*end*end - start * start*start)*quad / 3;
	}
	if (linear != 0)
	{
		ret += (end*end - start * start)*linear / 2;
	}
	if (con != 0)
	{
		ret += (end - start)*con;
	}

	return ret;
}

void BSpline::BesselTanget(const Point& p0, const Point& p1, const Point& p2,
	Vec3d& p0deriv, Vec3d& p1deriv, Vec3d& p2deriv)
{
	double delta_t1 = PointDistance(p2, p1);
	double delta_t0 = PointDistance(p1, p0);
	Vec3d delta_p0 = (p1 - p0) / delta_t0;
	Vec3d delta_p1 = (p2 - p1) / delta_t1;
	double sum = delta_t0 + delta_t1;
	p1deriv = delta_t1 / sum * delta_p0 + delta_t0 / sum * delta_p1;
	p0deriv = 2 * delta_p0 - p1deriv;
	p2deriv = 2 * delta_p1 - p1deriv;
}

void BSpline::BasisFunc(double u, int k, std::vector<double>& basis_func)
{
	const int& p = m_nDegree;
	const std::vector<double>& knots = m_vecKnots;
	basis_func.resize(p + 1);

	//2p+1�� N_{i,0}
	int n = 2 * p + 1;
	vector<double> temp(n, 0);
	temp[p] = 1;

	//����p��
	for (int j = 1; j <= p; ++j)
	{
		//���� [k-p,k+p+1)
		for (int i = k - p, h = 0; h < (n - j); ++h, ++i)
		{
			//���ƹ�ʽ
			double a = (u - knots[i]);
			double dev = (knots[i + j] - knots[i]);
			a = (dev != 0) ? a / dev : 0;

			double b = (knots[i + j + 1] - u);
			dev = (knots[i + j + 1] - knots[i + 1]);
			b = (dev != 0) ? b / dev : 0;

			temp[h] = a * temp[h] + b * temp[h + 1];
		}
	}
	//����ǰ p+1��ֵ��basis_func
	std::copy(temp.begin(), temp.begin() + p + 1, basis_func.begin());
}

Point BSpline::Evaluate(double u)
{
	const std::vector<double>& knots = m_vecKnots;
	int p = m_nDegree;

	//���� u ��������
	int k = std::distance(knots.begin(), std::upper_bound(knots.begin(), knots.end(), u)) - 1;

	//���� p+1��point,�ֱ��� P_{k-p},...,P_k
	Point *pArray = new Point[p + 1];
	std::copy(m_vecCVs.begin() + k - p, m_vecCVs.begin() + k + 1, pArray);

	//���� p��
	for (int r = 1; r <= p; ++r)
	{
		//i �� k �� k-p+1
		for (int i = k, j = p; i >= k - p + r; --i, --j)
		{
			double alpha = u - knots[i];
			double dev = (knots[i + p + 1 - r] - knots[i]);
			alpha = (dev != 0) ? alpha / dev : 0;

			pArray[j] = (1.0 - alpha)*pArray[j - 1] + alpha * pArray[j];
		}
	}

	//����������һ��
	Point pt = pArray[p];
	delete[]pArray;
	return pt;
}

void BSpline::InsertKnot(double u)
{
	std::vector<double>& knots = m_vecKnots;
	int p = m_nDegree;

	//���� u ��������
	int k = std::distance(knots.begin(), std::upper_bound(knots.begin(), knots.end(), u)) - 1;
	Point *pArray = new Point[p];

	//i \in [k,k-p+1]
	//jΪ����pArrayλ��
	for (int i = k, j = p - 1; i > k - p; --i, --j)
	{
		double alpha = (u - knots[i]);
		double dev = (knots[i + p] - knots[i]);
		alpha = (dev != 0) ? alpha / dev : 0;

		pArray[j] = m_vecCVs[i - 1] * (1 - alpha) + m_vecCVs[i] * alpha;
	}

	//��cv [k-p+1,k-1]�滻ΪpArray��������cv[k]֮ǰ���� pArray[p-1]
	for (int i = k - p + 1, j = 0; i < k; ++i, ++j)
	{
		m_vecCVs[i] = pArray[j];
	}
	m_vecCVs.insert(m_vecCVs.begin() + k, pArray[p - 1]);

	//knots ����u
	knots.insert(knots.begin() + k + 1, u);

	delete[] pArray;
}

double BSpline::PointDistance(const Point &arg1, const Point &arg2)
{
	return std::sqrt(std::pow(arg1(0) - arg2(0),2)
		+ std::pow(arg1(1) - arg2(1), 2)
		+ std::pow(arg1(2) - arg2(2), 2));
}

void BSpline::Tesselation(double tolerance, std::vector<Point>& points)
{
	std::vector<Point> cv_copy = m_vecCVs;
	std::vector<double> knots_copy = m_vecKnots;
	_tesselation(cv_copy, knots_copy, points, tolerance, m_nDegree, m_vecKnots.size() - m_nDegree, 0, m_vecCVs.size() - 1);
}

void BSpline::_tesselation(std::vector<Point>& cv, std::vector<double>& knots, std::vector<Point>& points, double tolerance, int k_s, int k_e, int c_s, int c_e)
{
	int p = m_nDegree;

	bool stright_enough = true;
	//�����Ҹ��Ƿ񳬹��ݲ�
	for (int i = c_s + 1; i < c_e; ++i)
	{
		//�㵽ֱ�߾��빫ʽ��������ʵ����
		if (PointLineDistance(cv[i], cv[c_s], cv[c_e]) > tolerance)
		{
			stright_enough = false;
			break;
		}
	}

	//����Ҫ�󣬲���һ��ϸ����
	if (stright_enough)
	{
		//Ϊ�˱�֤���Ƶ㲻�ظ�����ƵĹ���Ϊ[),���Ƕ����һ�������⡣
		//���յݹ�˳�����һ�����ȼ���points
		int c_end = points.empty() ? c_e + 1 : c_e;
		points.insert(points.begin(), cv.begin() + c_s, cv.begin() + c_end);
		return;
	}

	//�ӽڵ��м���
	double u_mid = knots[k_s] + (knots[k_e] - knots[k_s]) / 2.0;
	//���� u ��������
	int k = std::distance(knots.begin(), std::upper_bound(knots.begin(), knots.end(), u_mid)) - 1;

	std::vector<Point> cv_left, cv_right;
	_subdeviding(u_mid, k, knots, cv, cv_left, cv_right);

	//�ڵ���������p��u_mid
	knots.insert(knots.begin() + k + 1, p, u_mid);
	//���Ƶ��滻
	cv.insert(cv.begin() + k, p, Point());
	for (int i = k - p, j = 0; j < p; ++j, ++i)
		cv[i] = cv_left[j];

	for (int i = k, j = 0; j <= p; ++j, ++i)
		cv[i] = cv_right[j];

	//�����ֱַ�ݹ�
	//Note:��벿����ǰ�벿��֮ǰִ�У�
	//��Ϊ���ǰ�벿������ִ�еĻ�����벿�ֵ������ͷ����ı���
	_tesselation(cv, knots, points, tolerance, k + 1, k_e + p, k, c_e + p);
	_tesselation(cv, knots, points, tolerance, k_s, k + 1, c_s, k);
}

void BSpline::Subdeviding(double u, BSpline& sub_left, BSpline& sub_right)
{
	//Ϊ��ʹ�÷���
	const std::vector<double>& knots = m_vecKnots;
	const std::vector<Point>& cv = m_vecCVs;
	int p = m_nDegree;

	//���� u ��������
	int k = std::distance(knots.begin(), std::upper_bound(knots.begin(), knots.end(), u)) - 1;

	//����de boor �㷨�������������ɵĿ��Ƶ�
	std::vector<Point> cv_left, cv_right;
	_subdeviding(u, k, knots, cv, cv_left, cv_right);

	//������u����ϣ������µĿ��Ƶ����� ����ԭ���ߵĿ��Ƶ�����Ϊ P_0,...,P_k-p-1,P_k-p,...,P_k,P_k+1,...,P_n
	//��Ϻ���¿��Ƶ�����Ϊ����ࣺP_0,....,P_k-p-1,cv_left[...]���Ҳ� cv_right[...],P_k+1,...,P_n��
	//sub_left
	sub_left.m_nDegree = p;
	//knots
	sub_left.m_vecKnots.resize(k + p + 2);
	for (int i = 0; i < k + 1; ++i)
		sub_left.m_vecKnots[i] = knots[i];
	for (int i = 0, j = k + 1; i < p + 1; ++i, ++j)
		sub_left.m_vecKnots[j] = u;
	//control vertex
	sub_left.m_vecCVs.resize(k + 1);
	for (int i = 0; i < k - p; ++i)
		sub_left.m_vecCVs[i] = cv[i];
	for (int i = 0, j = k - p; i < p + 1; ++i, ++j)
		sub_left.m_vecCVs[j] = cv_left[i];

	//sub_right
	sub_right.m_nDegree = p;
	//knots
	sub_right.m_vecKnots.resize(knots.size() - k + p);
	for (int i = 0; i < p + 1; i++)
		sub_right.m_vecKnots[i] = u;
	for (int i = p + 1, j = k + 1; j < knots.size(); ++j, ++i)
		sub_right.m_vecKnots[i] = knots[j];
	//control_vertex
	sub_right.m_vecCVs.resize(cv.size() - k + p);
	for (int i = 0; i < p + 1; ++i)
		sub_right.m_vecCVs[i] = cv_right[i];
	for (int i = k + 1, j = p + 1; i < cv.size(); ++i, ++j)
		sub_right.m_vecCVs[j] = cv[i];
}

void BSpline::_subdeviding(double u, int k, const std::vector<double>& knots, const std::vector<Point>& cv,
	std::vector<Point>& cv_left, std::vector<Point>& cv_right)
{
	int p = m_nDegree;

	//����de boor �㷨�������������ɵĿ��Ƶ�
	cv_left.resize(p + 1);
	cv_right.resize(p + 1);

	//�� P_k-p,...,P_k������cv_left����
	std::copy(cv.begin() + k - p, cv.begin() + k + 1, cv_left.begin());
	cv_right[p] = cv_left[p];

	//de-boor ����p��
	for (int r = 1; r <= p; ++r)
	{
		//i �� k �� k-p+1
		for (int i = k, j = p; i >= k - p + r; --i, --j)
		{
			double alpha = u - knots[i];
			double dev = (knots[i + p + 1 - r] - knots[i]);
			alpha = (dev != 0) ? alpha / dev : 0;

			cv_left[j] = (1.0 - alpha)*cv_left[j - 1] + alpha * cv_left[j];
		}

		cv_right[p - r] = cv_left[p];
	}
}

double BSpline::PointLineDistance(const Point &s, const Point &a, const Point &b)
{
	double ab = sqrt(pow((a(0) - b(0)), 2.0) + pow((a(1) - b(1)), 2.0) + pow((a(2) - b(2)), 2.0));
	double as = sqrt(pow((a(0) - s(0)), 2.0) + pow((a(1) - s(1)), 2.0) + pow((a(2) - s(2)), 2.0));
	double bs = sqrt(pow((s(0) - b(0)), 2.0) + pow((s(1) - b(1)), 2.0) + pow((s(2) - b(2)), 2.0));
	double cos_A = (pow(as, 2.0) + pow(ab, 2.0) - pow(bs, 2.0)) / (2 * ab*as);
	double sin_A = sqrt(1 - pow(cos_A, 2.0));
	return as * sin_A;
}

/*!
 *\brief ����B������ֵ
*\ param const std::vector<Point> & vecFitPoints����ֵ�㼯�ϣ���Ҫ������С��3
*\ Returns:   BSpline ��ֵ��������
*/
BSpline BSpline::CubicInterpolate(const std::vector<Point>& vecFitPoints)
{
	const int p = 3;
	BSpline bs;
	int x = vecFitPoints.size();
	if (x < p)
	{
		cout << "too less point !" << endl;
		return bs;
	}

	//��ⷽ�� N*P = F
	Eigen::MatrixXd N = Eigen::MatrixXd::Zero(x + 2, x + 2);
	Eigen::MatrixXd P = Eigen::MatrixXd::Zero(x + 2, 3);
	Eigen::MatrixXd F = Eigen::MatrixXd::Zero(x + 2, 3);

	bs.m_nDegree = p;
	bs.m_vecKnots.resize(x); //x+6���ڵ�
	//����ڵ�
	bs.m_vecKnots[0] = 0.0;
	for (int i = 1; i < x; ++i)
	{
		bs.m_vecKnots[i] = bs.m_vecKnots[i - 1]
			+ PointDistance(vecFitPoints[i], vecFitPoints[i - 1]);
	}
	//�ڵ���β����p+1���ظ�
	bs.m_vecKnots.insert(bs.m_vecKnots.begin(), p, bs.m_vecKnots.front());
	bs.m_vecKnots.insert(bs.m_vecKnots.end(), p, bs.m_vecKnots.back());

	//1.��д����N
	std::vector<double> basis_func;
	N(0, 0) = 1;
	N(x - 1, x + 1) = 1;
	for (int i = p + 1; i < x + p - 1; ++i)
	{
		//c(u)�� N_{i-p},...,N_i��p+1���������Ϸ���
		bs.BasisFunc(bs.m_vecKnots[i], i, basis_func);
		for (int j = i - p, k = 0; j <= i; ++j, ++k)
		{
			N(i - p, j) = basis_func[k];
		}
	}

	//����
	N(x, 0) = -1;
	N(x, 1) = 1;
	N(x + 1, x) = -1;
	N(x + 1, x + 1) = 1;

	//2.��д����F
	for (int i = 0; i < x; ++i)
	{
		F(i, 0) = vecFitPoints[i](0);
		F(i, 1) = vecFitPoints[i](1);
		F(i, 2) = vecFitPoints[i](2);
	}

	{
		Vec3d v0, v1, v2;
		BesselTanget(vecFitPoints[0], vecFitPoints[1], vecFitPoints[2], v0, v1, v2);
		Vec3d v = v0 * (bs.m_vecKnots[p + 1] - bs.m_vecKnots[1]) / (double)p;
		F(x, 0) = v(0);
		F(x, 1) = v(1);
		F(x, 2) = v(2);
	}

	{
		Vec3d v0, v1, v2;
		BesselTanget(vecFitPoints[x - 3], vecFitPoints[x - 2], vecFitPoints[x - 1], v0, v1, v2);
		Vec3d v = v2 * (bs.m_vecKnots[x + 1 + p] - bs.m_vecKnots[x + 1]) / (double)p;
		F(x + 1, 0) = v(0);
		F(x + 1, 1) = v(1);
		F(x + 1, 2) = v(2);
	}

	//�ⷽ�� N*P = F
	P = N.lu().solve(F);

#ifdef _DEBUG
	cout << "N--------------" << endl << N << endl;
	cout << "F--------------" << endl << F << endl;
	cout << "P--------------" << endl << P << endl;
#endif

	//��Eigen����Ľ������bs��control_vertex
	bs.m_vecCVs.resize(x + 2);
	for (int i = 0; i < x + 2; ++i)
	{
		Point& cv = bs.m_vecCVs[i];
		cv(0) = P(i, 0);
		cv(1) = P(i, 1);
		cv(2) = P(i, 2);
	}

	return bs;
}