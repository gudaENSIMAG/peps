#include "StdAfx.h"
#include "MC_Compute.h"
#include "test.h"



MC_Compute::MC_Compute(Produit * produit, Model * model)
{
	m_discretisation = WEEK;
	m_produit = produit;

	//Sert � simplifier la lecture du code. 
	m_sizeEquityProduct = produit->getEquities().size();
	m_model = model;

	m_rng = pnl_rng_create (PNL_RNG_MERSENNE);
	pnl_rng_sseed (m_rng, time(NULL));

	//Vecteur de date de fixing
	static const int arr[] = {FIXING0, FIXING1, FIXING2, FIXING3, FIXING4};
	std::vector<int> lvec_fixingDate (arr, arr + sizeof(arr) / sizeof(arr[0]) );
	mvec_fixingDate = lvec_fixingDate;
}


bool MC_Compute::isRemb(PnlMat * coursHisto, int time) {
	bool tmp = false;
	if(time > mvec_fixingDate[1]){
		if(Condition_Remb(coursHisto, mvec_fixingDate[1])){
			std::cout<< "LE PAYOFF A DEJA ETE TOUCHE EN t1"<<std::endl;
			return true;
		}
		if (time > mvec_fixingDate[2]){
			if(Condition_Remb(coursHisto, mvec_fixingDate[2])){
				std::cout<< "LE PAYOFF A DEJA ETE TOUCHE EN t2"<<std::endl;
				return true;
			}
			if (time > mvec_fixingDate[3]){
				if(Condition_Remb(coursHisto, mvec_fixingDate[3])){
					std::cout<< "LE PAYOFF A DEJA ETE TOUCHE EN t3"<<std::endl;
					return true;
				}
			}
		}
	}
	return tmp;
}


//TODO 
// Devise non g�r�
// a quel moment doit on passer le spot en EUR
// comment g�rer la maturit� en actualisant
// faire attention a diffuser en CZK mais que la valeur des actifs soient stock�es avec leur monnaie respective
int MC_Compute::Price(double * sumPrice, double *priceSquare, PnlVect * sumDelta, PnlVect * sumGamma, int time)
{
	//reset obligatoire des param�tre
	*sumPrice = 0;
	*priceSquare = 0;
	pnl_vect_set_zero(sumDelta);
	pnl_vect_set_zero(sumGamma);
	//check model parameter
	if(!m_model->CheckParameter()) return -1;
	//on recup�re cours histo ce n'est pas a nous de la d�sallouer
	PnlMat * l_coursHisto = m_produit->getMatHisto();

	// Payoff prendra le payoff du m_produit a chaque tour de boucle
	double l_payoff = 0;
	//ATTRIBUT A METTRE DANS EQUITY: on devra avoir une m�thode getinitdrift mais avant on doit creer la classe currency chaque currency a un drift
	PnlVect *l_drift = pnl_vect_create_from_double(m_sizeEquityProduct, 0.05);
	//On peu faire mieux ici
	PnlVect * l_spot = GetInitSpot();
	PnlVect * l_vol = GetInitVol();
	// La matrice past va contenir le passe (ie les valeurs historiques jusqua time)
	PnlMat * l_past = pnl_mat_create(l_coursHisto->m, l_coursHisto->n);
	TronqCoursHisto(l_coursHisto, l_past, time);

	if(isRemb(l_coursHisto, time))
		return -10;

	//MonteCarlo
	for (int i = 0; i < m_model->Nb_Path(); i++) {
		PnlMat *l_histoFix = pnl_mat_create(m_sizeEquityProduct, mvec_fixingDate.size());
		m_model->Diffuse_from_t(l_past, l_drift, l_vol, m_produit, m_rng, time);
		// en sortie la matrice past contient les valeurs historiques sur les colonnes de 0 a time
		// et les valeurs simulees de time + 1 a la derniere
		
		// A partir de Past on calcul le prix et le delta et le gamma

		// ICI APPELER la fonction qui a partir de past retourne la matrice juste au date de fixing
		getPathFix(l_past, l_histoFix, mvec_fixingDate);
		PriceProduct(l_histoFix, &l_payoff, time);
		ComputeGrec(sumDelta, sumGamma, l_past, l_payoff, l_vol, l_drift, time);
		*sumPrice += l_payoff;
		pnl_mat_free(&l_histoFix);
	}

	//Moyenne du prix
	*sumPrice /= m_model->Nb_Path();
	pnl_vect_div_double(sumDelta,  m_model->Nb_Path());
	pnl_vect_div_double(sumGamma,  m_model->Nb_Path());

	pnl_vect_free(&l_drift);
	pnl_vect_free(&l_spot);
	pnl_vect_free(&l_vol);
	pnl_mat_free(&l_past);

	return 0;
}

inline void MC_Compute::PriceProduct(const PnlMat * histoFix, double * payoff, int time) {
	// Renta est la matrice des rentabilites autant de ligne que d'actifs et 4 colonnes car il y a 4 intervales de discretisation (5dates)
	PnlMat *l_renta = pnl_mat_create(m_sizeEquityProduct, PAS);
	// On calcul le prix du m_produit //Pour ca on a besoin de la rentabilite du portefeuille
	Rent(histoFix, l_renta);
	// On peut ensuite calculer son payoff //Ici la fonction price2 devrait retourner la date ou le payoff a ete touche
	*payoff = Price2(l_renta, time);
	pnl_mat_free(&l_renta);
}


inline void MC_Compute::ComputeGrec(PnlVect * sumDelta, PnlVect* sumGamma, const PnlMat * past, const double payoff, PnlVect* l_vol, PnlVect* l_drift, int time) {
	
	PnlMat *l_pastShPos = pnl_mat_create(past->m, past->n);
	PnlMat *l_pastShNeg = pnl_mat_create(past->m, past->n);
	PnlMat *l_histoFixShPos = pnl_mat_create(m_sizeEquityProduct, mvec_fixingDate.size());
	PnlMat * l_histoFixShNeg = pnl_mat_create(m_sizeEquityProduct, mvec_fixingDate.size());
	PnlMat *l_rentPos = pnl_mat_create(m_sizeEquityProduct, mvec_fixingDate.size() -1);
	PnlMat *l_rentNeg = pnl_mat_create(m_sizeEquityProduct, mvec_fixingDate.size()- 1);
	PnlVect *l_gamma = pnl_vect_create(m_sizeEquityProduct);
	PnlVect *l_delta = pnl_vect_create(m_sizeEquityProduct);
	double ld_payoffPos = 0;
	double ld_payoffNeg = 0;

	for (int l = 0; l < m_sizeEquityProduct; l++) {
			pnl_mat_clone(l_pastShPos, past);
			pnl_mat_clone(l_pastShNeg, past);
			for (int n = time; n < PAS; n++){
				// On shifte positivement et negativement
				MLET(l_pastShPos,l,n)=(MGET(past,l,n))*(1+H);
				MLET(l_pastShNeg,l,n)=(MGET(past,l,n))*(1-H);
			}
			// ICI appeler la fonction qui selectionne les dates de fixing et qui retroune l_histoFixShPos et l_histoFixShNeg
			getPathFix(l_pastShPos, l_histoFixShPos, mvec_fixingDate);
			getPathFix(l_pastShNeg, l_histoFixShNeg, mvec_fixingDate);
	
			//reset matrice
			pnl_mat_set_zero(l_pastShNeg);
			pnl_mat_set_zero(l_rentPos);
			
			//calcul des rentabilites pour chacune des matrices de histofix shifte
			Rent(l_histoFixShPos,l_rentPos);
			Rent(l_histoFixShNeg,l_rentNeg);

			// Calcul le payoff a partir de la matrice des rentabilites
			ld_payoffPos = Price2(l_rentPos, time);
			ld_payoffNeg = Price2(l_rentNeg, time);		

			pnl_vect_set(l_delta, l, ((ld_payoffPos-ld_payoffNeg)/(2*H)));
			pnl_vect_set(l_gamma, l ,((ld_payoffPos - 2 * payoff + ld_payoffNeg)/(pow(H,2))));
			

	}
	// On somme les delta
	pnl_vect_plus_vect(sumDelta, l_delta);
	// On somme les gamma
	pnl_vect_plus_vect(sumGamma, l_gamma);

	//lib�ration ressource
	pnl_mat_free(&l_histoFixShPos);
	pnl_mat_free(&l_histoFixShNeg);
	pnl_vect_free(&l_gamma);
	pnl_vect_free(&l_delta);
	pnl_mat_free(&l_pastShPos);
	pnl_mat_free(&l_pastShNeg);
	pnl_mat_free(&l_rentPos);
	pnl_mat_free(&l_rentNeg);
}

MC_Compute::~MC_Compute()
{
}

// cette fonction prend en argument historique de fixing
// elle ressort le payoff du m_produit
void MC_Compute::PayOff(void)
{
}

inline double MC_Compute::Compute_dt(int date)
{
	if (m_discretisation = WEEK) {
		if (date < 104)
		{
			return (104 - date)/52;
		} else 
		{
			return (52 - (date % 52))/52;
		}
	} else {
		if (date < 104*7)
		{
			return (104*7 - date)/365;
		} else 
		{
			return (365 - (date % 365))/365;
		}
	}
}

void MC_Compute::Compute_path(void)
{// cette fonction prend en argument le temps et ressort la matrice de fixing
}

inline double MC_Compute::Discount(double value, int date, int time)
{
	//Pour le moment le taux sans risque est la c'est pas la qu'il devra etre
	return (value*exp(-(((double)abs(date-time))/NBSEMAINE * TAUX_ACTUALISATION)));
}

inline double MC_Compute::Price2(const PnlMat *rent, int time)
{
	double minCol;
	double val;
	for (int i = 0; i < rent->n; i++)
	{
		minCol = 0;
		for (int j = 0; j < rent->m; j++)
		{
			if (pnl_mat_get(rent, j, i) < minCol)
			{
				minCol = pnl_mat_get(rent, j, i);
			}
		}
		if ((minCol > -0.1) && (i != 3))
		{
		    return Discount(REMB_ANTI, mvec_fixingDate[i], time);
		}
	}
	PnlVect *perf = pnl_vect_create(rent->m);
	pnl_mat_get_col(perf, rent, rent->n - 1);
	val = std::max(1.0, REMB_N_ANTI + Perf_Liss(perf));
	pnl_vect_free(&perf);
	//return Discount(val, (rent->n -1), time);
	return Discount(val, mvec_fixingDate[mvec_fixingDate.size() - 1], time);
}

inline void MC_Compute::Perf_Boost(const PnlVect *perf, PnlVect * ret)
{
	double val;
	for (int i = 0; i < perf->size; i++)
	{
		val = pnl_vect_get(perf, i);
		if (val > 0)
		{
			val *= 2;
		}
		pnl_vect_set(ret, i, val);
	}
}

inline double MC_Compute::Perf_Liss(const PnlVect *spot)
{
	double val;
	double ret = 0;
	PnlVect *res =	 pnl_vect_create(spot->size);
	Perf_Boost(spot, res);
	for (int i = 0; i < res->size; i++)
	{
		val = pnl_vect_get(res, i);
		if (val < PERF_MIN)
		{
			val = PERF_MIN;
		} 
		else if (val > PERF_MAX)
		{
			val = PERF_MAX;
		}
		ret += val;
	}
	pnl_vect_free(&res);
	return ret/m_sizeEquityProduct;
}

inline void MC_Compute::Rent(const PnlMat *histoFix, PnlMat *res)
{
	double rent;
	for (int i = 0; i < histoFix->n - 1; i++)
	{
		for (int j = 0; j < NB_ACTIFS; j++)
		{
			rent = (pnl_mat_get(histoFix, j,  i + 1) / pnl_mat_get(histoFix, j, 0)) - 1;
			pnl_mat_set(res, j, i, rent);
		}
	}
}

//spot est a desallouer en dehors
PnlVect * MC_Compute::GetInitSpot() {
	PnlVect * l_spot = pnl_vect_create(m_sizeEquityProduct);
	for(int k = 0; k < m_sizeEquityProduct; k++){
		pnl_vect_set(l_spot, k, m_produit->getEquities()[k].value);
	}
	return l_spot;
}

//vol est a desallouer en dehors
PnlVect * MC_Compute::GetInitVol() {
	PnlVect *vol = pnl_vect_create(m_sizeEquityProduct);
	for(int k = 0; k < m_sizeEquityProduct; k++){
		pnl_vect_set(vol, k, m_produit->getEquities()[k].volatility);
	}
	return vol;	
}


inline void MC_Compute::RentVect(PnlVect * V, PnlVect * V0, PnlVect * res) {
	for (int k = 0 ; k < res->size; k++){
		pnl_vect_set(res, k, (pnl_vect_get(V,k)/pnl_vect_get(V0,k)- 1));
	}
}

inline bool MC_Compute::Condition_Remb(PnlMat * past, int time){
	bool condSortie = false;
	PnlVect * S0 = pnl_vect_create(past->m);
	PnlVect * S1 = pnl_vect_create(past->m);
	PnlVect * rent = pnl_vect_create(past->m);
	pnl_mat_get_col(S0, past, 0);
	pnl_mat_get_col(S1, past, time);
	RentVect(S1, S0, rent);
	for (int j = 0; j< rent->size; j++){
		if ( pnl_vect_get(rent, j) > -0.1 ) condSortie = true;
	}
	pnl_vect_free(&S0);
	pnl_vect_free(&S1);
	pnl_vect_free(&rent);
	return condSortie;
}
