#pragma once
#include "Produit.h"
#include "Model.h"
#include <pnl/pnl_random.h>

class MC_Compute
{
private:

	enum DISCRETISATION_TYPE { DAY, WEEK };

	double T;
	double K;
	double m_spot;
	double drift;
	double vol;
	PnlRng * m_rng;
	Produit * m_produit;
	Model * m_model;
	int m_sizeEquityProduct;
	DISCRETISATION_TYPE m_discretisation;
	/*
	* D�claration du vecteur spot
	* return vecteur des prix spot
	*/
	PnlVect * MC_Compute::GetInitSpot();
	/*
	* D�claration du vecteur volatilit�
	* return vecteur volatitli�
	*/
	PnlVect * MC_Compute::GetInitVol();
	/*
	*	Check if all paramater if define don't start price if not
	*/
	bool MC_Compute::CheckParameter(); 
	/*
	* @param out matrice des delta
	* @param out matrice des gamma
	* @param in matricte histofix
	* @param in payoff
	*/
    inline void MC_Compute::ComputeGrec(PnlVect * sumDelta, PnlVect* sumGamma, const PnlMat * histoFix, const double payoff, PnlVect* l_vol, PnlVect* l_drift, int time);


	/*
	* @param histoFix in
	* @param payoff out
	*/
	inline void MC_Compute::PriceProduct(const PnlMat * histoFix, double * payoff, int time);
	inline double Price2(const PnlMat *rent, int time);
	inline double Discount(double value, int date, int time);
	inline double Perf_Liss(const PnlVect *spot);
	/*
	in
	out
	*/
	inline void MC_Compute::Perf_Boost(const PnlVect *perf, PnlVect * ret);
	//Fonction qui calcule les rentabilit�s � chaque date de constatation
	inline void Rent(const PnlMat *histoFix, PnlMat *res);
	/*double MC_Compute::Diffusion(const double spot, const double drift, const double dt, const double nbAlea, const PnlMat *matCor, const  Produit produit, const int equity);
	double MC_Compute::ComputeNextStep(const double drift, 
								   const double volatility, 
								   const double dt, 
								   const double nbAlea,
								   const PnlMat *matCor);*/


	bool MC_Compute::isRemb(PnlMat * coursHisto, int time);

	void MC_Compute::RentVect(PnlVect * V, PnlVect * V0, PnlVect * res);

public :
	MC_Compute(Produit * produit, Model * model);
	~MC_Compute();

	std::vector<int> mvec_fixingDate;

	/*
	* out price
	* out pricesquare
	* out Delta
	* out gamma
	*/
	int MC_Compute::Price(double * sumPrice, double *priceSquare, PnlVect * sumDelta, PnlVect * sumGamma, int time); 
	bool MC_Compute::Condition_Remb(PnlMat * past, int time);
	void payoff();
	double Compute_dt(int date);
	void Compute_path();
	void PayOff();

	
};


