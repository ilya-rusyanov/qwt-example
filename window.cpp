#include <iomanip> // setw
#include <QThread>
#include <unistd.h>
#include <QMutex>
#include <iostream> // cout
#include "window.h"

#include <cmath>  // for sine stuff

template<typename T>
T dF_Adams(//! текущая производная аргумента
           const T& ut,
           //! производная аргумента с предыдущего шага
           T* ut0,
           //! шаг интегрирования по времени
           const double& Stp)
{
  // что изначально делает эта процедура:
  /////
  // T temp = (- Stp*.5*(*ut0) + Stp*1.5*(ut));
  // *ut0 = ut;
  // return temp;
  /////

  T temp;
  {
    T ut0_m;
    {
      ut0_m = *ut0;
      ut0_m *= - (Stp * .5);
    }

    T ut_m;
    {
      ut_m = ut;
      ut_m *= (Stp * 1.5);
    }

    temp = (ut0_m += ut_m);
  }

  *ut0 = ut;

  return temp;
}

class Window::Calculator
{
        class Calc: public QThread
        {
                Calculator* m_parent;

                struct Phys
                {
                    Phys():
                        time(0),
                        a(0),
                        aPrev(0),
                        v(0),
                        vPrev(0),
                        x(0)
                    {}

                    double time;

                    double a;

                    double aPrev;

                    double v;

                    double vPrev;

                    double x;

                };

                class Frict
                {
                    public:
                        Frict():
                            gamma1(40.25),
                            gamma2(7),
                            gamma3(1),
                            gamma4(20),
                            gamma5(6),
                            gamma6(0.01)
                        {}

                        double m(double speed)
                        {
                            return gamma1 * (std::tanh(gamma2 * speed) - std::tanh(gamma3 * speed)) + gamma4 * std::tanh(gamma5*speed) + gamma6 * speed;
                        }

                        double gamma1;
                        double gamma2;
                        double gamma3;
                        double gamma4;
                        double gamma5;
                        double gamma6;
                };

                Frict frict;

                Phys phys;

                bool stopFlag;

            public:
                Calc(Calculator* parent):
                    m_parent(parent),
                    stopFlag(false),
                    time(0),
                    ef(0)
                {}

                double val(double& t, double& s, double& spring)
                {
                    t = time;
                    s = phys.v;
                    spring = 30000 * (0.15 * tanh(phys.x * 30) + 3.47055 * tanh(phys.x * phys.x));
                    return phys.x;
                }

                void setStop()
                {
                    stopFlag = true;
                }

                void setExtF(double f)
                {
                    ef = f;
                }

                double time;

                double ef;

            protected:
                virtual void run()
                {
                    while(!stopFlag)
                    {
                        const double step = 0.001;

                        const double m = 30000;
                        const double k = m * (0.15 * tanh(phys.x * 30) + 3.47055 * tanh(phys.x * phys.x));
                        const double Fsum = ef + m * 9.8 - 9.8 * k - 0.1 * m * frict.m(phys.v);
                        phys.a = Fsum / m;
                        phys.v += dF_Adams(phys.a, &phys.aPrev, step);
                        m_parent->lock.lock();
                        phys.x += dF_Adams(phys.v, &phys.vPrev, step);
                        time += step;
                        m_parent->lock.unlock();
                        usleep(1./step);
                    }

                }

        };

        Calc calc;

        QMutex lock;

    public:
        Calculator():
            calc(this)
        {
            calc.start();
        }

        ~Calculator()
        {
            calc.setStop();
            calc.wait();
        }

        double get(double& time, double& speed, double& springf)
        {
            lock.lock();
            double rval = calc.val(time, speed, springf);
            lock.unlock();
            return rval;
        }

        void setForce(double f)
        {
            calc.setExtF(f);
        }


};


Window::Window() : plot( QString("Example Plot") ), gain(0), count(0),
    calculator(new Calculator())
{
	// set up the gain knob
	knob.setValue(gain);
    knob.setScale(-50, +50);

	// use the Qt signals/slots framework to update the gain -
	// every time the knob is moved, the setGain function will be called
	connect( &knob, SIGNAL(valueChanged(double)), SLOT(setGain(double)) );

	// set up the thermometer
	thermo.setFillBrush( QBrush(Qt::red) );
//	thermo.setRange(0, 20);
	thermo.show();


	// set up the initial plot data
	for( int index=0; index<plotDataSize; ++index )
	{
		xData[index] = index;
		yData[index] = 0;//gain * sin( M_PI * index/50 );
	}

	// make a plot curve from the data and attach it to the plot
	curve.setSamples(xData, yData, plotDataSize);
	curve.attach(&plot);

	plot.replot();
	plot.show();


	// set up the layout - knob above thermometer
	vLayout.addWidget(&knob);
	vLayout.addWidget(&thermo);

	// plot to the left of knob and thermometer
	hLayout.addLayout(&vLayout);
	hLayout.addWidget(&plot);

	setLayout(&hLayout);
}

Window::~Window()
{
    delete calculator;
}

void Window::timerEvent( QTimerEvent * )
{
    //phys.time += 0.0001;
	// generate an sine wave input for example purposes - you must get yours from the A/D!
	double inVal = gain;
	++count;

    //std::cout << "a: " << phys.a << ", v: " << phys.v << ", x: " << phys.x << ", Fspring: " << Fspring << ", frict: " << frict.m(phys.v) << "\n";

	// add the new input to the plot
    calculator->setForce(inVal * 10000 * 9.8);
	memmove( yData, yData+1, (plotDataSize-1) * sizeof(double) );
    double time;
    double speed;
    double springf;
    const double x = calculator->get(time, speed, springf);
	yData[plotDataSize-1] = x;
    const int w = 12;
    std::cout << "t: " << std::setw(w) << time << ", x: " << std::setw(w) << x << ", v: " << std::setw(w) << speed << " springf: " << std::setw(w) << springf << "\n";
	//memmove( xData, xData+1, (plotDataSize-1) * sizeof(double) );
	//xData[plotDataSize-1] = phys.time;
	curve.setSamples(xData, yData, plotDataSize);
	plot.replot();

	// set the thermometer value
	thermo.setValue( 0 );
}


// this function can be used to change the gain of the A/D internal amplifier
void Window::setGain(double gain)
{
	// for example purposes just change the amplitude of the generated input
	this->gain = gain;
}
