/**
cnt.cpp
Stores all relevant information for a carbon nanotube
*/

#include <iostream>
#include <numeric>
#include <armadillo>
#include <complex>
#include <stdexcept>

#include "constants.h"
#include "cnt.h"

// set the output directory and the output file name
void cnt::process_command_line_args(int argc, char* argv[])
{
  namespace fs = std::experimental::filesystem;

  // first find the xml input file and open it
  fs::directory_entry xml_file;
  std::cout << "current path is " << fs::current_path() << std::endl;

  if (argc <= 1)
  {
    xml_file.assign("input.xml");
  }
  else
  {
    xml_file.assign(argv[1]);
  }

  if(fs::exists(xml_file))
  {
    std::cout << "input xml file found: " << xml_file.path() << std::endl;
  }
  else
  {
    std::cout << "input xml file NOT found: " << xml_file.path() << std::endl;
    std::exit(1);
  }

  if (!fs::is_regular_file(xml_file))
  {
    std::cout << "input xml file NOT found: " << xml_file.path() << std::endl;
    std::exit(1);
  }
  std::cout << std::endl;

  rapidxml::file<> xmlFile(xml_file.path().c_str()); //open file
  rapidxml::xml_document<> doc; //create xml object
  doc.parse<0>(xmlFile.data()); //parse contents of file
  rapidxml::xml_node<>* curr_node = doc.first_node(); //gets the node "Document" or the root nodes
  curr_node = curr_node->first_node();

  // get the sibling node with name sibling_name
  auto get_sibling = [](rapidxml::xml_node<>* node, const std::string& sibling_name)
  {
    if (node->name() == sibling_name)
    {
      return node;
    }
    auto next_node = node->next_sibling(sibling_name.c_str());
    if (next_node == 0)
    {
      next_node = node->previous_sibling(sibling_name.c_str());
      if (next_node == 0)
      {
        std::cout << "sibling not found: " << sibling_name.c_str() << std::endl;
        std::exit(1);
      }
    }
    return next_node;
  };

  // get name cnt name
  {
    curr_node = get_sibling(curr_node, "name");
    _name = trim_copy(curr_node->value());
    std::cout << "cnt name: '" << _name << "'\n";
  }

  // set the output_directory
  {
    curr_node = get_sibling(curr_node,"output_directory");

    std::string attr = curr_node->first_attribute("type")->value();
    // std::string path = trim_copy(curr_node->value());
    fs::path path = trim_copy(curr_node->value());
    if (attr == "absolute")
    {
      std::cout << "absolute directory format used!\n";
    }

    _directory.assign(path);
    std::cout << "output_directory: " << _directory.path() << std::endl;

    if (not fs::exists(_directory.path()))
    {
      std::cout << "warning: output directory does NOT exist!!!" << std::endl;
      std::cout << "output directory: " << _directory.path() << std::endl;
      fs::create_directories(_directory.path());
    }

    if (fs::is_directory(_directory.path()))
    {
      if (not fs::is_empty(_directory.path()))
      {
        std::cout << "warning: output directory is NOT empty!!!" << std::endl;
        std::cout << "output directory: " << _directory.path() << std::endl;
        std::cout << "deleting the existing directory!!!" << std::endl;
        fs::remove_all(_directory.path());
        fs::create_directories(_directory.path());
      }
    }
    else
    {
      std::cout << "error: output path is NOT a directory!!!" << std::endl;
      std::cout << "output path: " << _directory.path() << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  // read chirality
  {
    curr_node = get_sibling(curr_node,"chirality");
    std::string chirality = curr_node->value();
    _n = std::stoi(chirality.substr(0,chirality.find(",")));
    _m = std::stoi(chirality.substr(chirality.find(",")+1));
    std::cout << "chirality: (" << _n << "," << _m << ")\n";
  }

  // length of cnt
  {
    curr_node = get_sibling(curr_node,"length");
    std::string units = curr_node->first_attribute("units")->value();
    if (units != "cnt_unit_cell")
    {
      std::cout << "cnt length is not in units '" << units << "'\n";
      std::exit(1);
    }
    _number_of_cnt_unit_cells = std::stoi(curr_node->value());
    std::cout << "length of cnt: " << _number_of_cnt_unit_cells << " unit cells.\n";
  }

  std::cout << std::endl;

};

void cnt::get_parameters()
{
  // graphen unit vectors and reciprocal lattice vectors
  _a1 = arma::vec({_a_l*std::sqrt(3.0)/2.0, +_a_l/2.0});
  _a2 = arma::vec({_a_l*std::sqrt(3.0)/2.0, -_a_l/2.0});
  _b1 = arma::vec({1.0/sqrt(3.0)*2.0*constants::pi/_a_l, +2.0*constants::pi/_a_l});
  _b2 = arma::vec({1.0/sqrt(3.0)*2.0*constants::pi/_a_l, -2.0*constants::pi/_a_l});

  // carbon-carbon translation vector
  _aCC_vec = 1.0/3.0*(_a1+_a2);

  // cnt chirality vector and its length
	_ch_vec = double(_n) * _a1 + double(_m) * _a2;
	_ch_len = arma::norm(_ch_vec,2);

  // cnt radius
  _radius = _ch_len/2.0/constants::pi;

  // calculate cnt t_vector
  int dR = std::gcd(2*_n+_m,_n+2*_m);
	_t1 = +(2*_m+_n)/dR;
	_t2 = -(2*_n+_m)/dR;
  _t_vec = double(_t1)*_a1 + double(_t2)*_a2;

  // number of hexagons in cnt unit cells which is equal to number of carbon atoms divided by half
  _Nu = 2*(std::pow(_n,2)+std::pow(_m,2)+_n*_m)/dR;


  // rotate basis vectors so that ch_vec is along the x_axis and t_vec is along y_axis
	double cos_theta = _ch_vec(0)/arma::norm(_ch_vec);
	double sin_theta = _ch_vec(1)/arma::norm(_ch_vec);
	arma::mat rot = {{+cos_theta, +sin_theta},
                   {-sin_theta, +cos_theta}}; // rotation matrix

	_ch_vec = rot*_ch_vec;
	_t_vec = rot*_t_vec;
	_a1 = rot*_a1;
	_a2 = rot*_a2;
	_b1 = rot*_b1;
	_b2 = rot*_b2;
	_aCC_vec = rot*_aCC_vec;

  	//make 3d t_vec where the cnt axis is parallel to y-axis
	_t_vec_3d = arma::vec(3,arma::fill::zeros);
	_t_vec_3d(1) = _t_vec(1);


  std::cout << "\n...graphene unit cell vectors:\n";
  _a1.print("a1:");
  _a2.print("a2:");

  std::cout << "\n...graphene reciprocal lattice vectors:\n";
  _b1.print("b1:");
  _b2.print("b2:");

  std::cout << "\n...vector connecting basis carbon atoms:\n";
  _aCC_vec.print("aCC vector:");

  _ch_vec.print("chirality vector:");
  std::cout << "ch_vec length:\n   " << _ch_len << std::endl;

  _t_vec.print("t_vec:");
  _t_vec_3d.print("3d t_vec:");


	// calculate reciprocal lattice of CNT
	_K1 = (-double(_t2)*_b1 + double(_t1)*_b2)/(double(_Nu));
	_K2 = (double(_m)*_b1-double(_n)*_b2)/(double(_Nu));
  _K2_normed = arma::normalise(_K2);
  _nk_K1 = _number_of_cnt_unit_cells;
	_dk_l = _K2/(double(_nk_K1));

  std::cout << "\n...cnt reciprocal lattice vectors:\n";
  _K1.print("K1:");
  _K2.print("K2:");

	// calculate K2-extended representation parameters
  {
    double p_min = (1./double(_t1)+1./double(_n))/(double(_m)/double(_n)-double(_t2)/double(_t1));
    double p_max = (1./double(_t1)+double(_Nu)/double(_n))/(double(_m)/double(_n)-double(_t2)/double(_t1));

    double q_min = double(_t2)/double(_t1)*p_max + 1./double(_t1);
    double q_max = double(_t2)/double(_t1)*p_min + 1./double(_t1);

    bool found = false;

    for (int p=std::ceil(p_min); p<std::ceil(p_max); p++)
    {
      if (((1+_t2*p) % _t1) == 0)
      {
        int q = (1+_t2*p)/_t1;
        _M = _m*p - _n*q;
        _Q = std::gcd(_Nu,_M);
        std::cout << "\n...K2-extended representation parameters:\n M: " << _M << " ,Q: " << _Q << "\n";
        found = true;
        break;
      }
    }
    if (not found)
    {
      std::cout << "Failed to calculate p and q for K2-extended representation .... investigate! .... aborting the simulation!!!\n";
    }
  }

}

// calculates position of atoms and reciprocal lattice vectors
void cnt::get_atom_coordinates()
{
  
	// calculate positions of atoms in the cnt unit cell
	_pos_a = arma::mat(_Nu,2,arma::fill::zeros);
	_pos_b = arma::mat(_Nu,2,arma::fill::zeros);

	int k = 0;

	for (int i=0; i<=_t1+_n; i++)
	{
		for (int j=_t2; j<=_m; j++)
		{
			bool flag1 = double(_t2*i)/(double)_t1 <= double(j);
			bool flag2 = double(_m*i)/(double)_n >= double(j);
			bool flag3 = double(_t2*(i-_n))/double(_t1) > double(j-_m);
			bool flag4 = double(_m*(i-_t1))/double(_n) < double(j-_t2);

			if(flag1 && flag2 && flag3 && flag4)
			{
        _pos_a.row(k) = double(i)*_a1.t() + double(j)*_a2.t();
        _pos_b.row(k) = _pos_a.row(k) + _aCC_vec.t();

				if(_pos_a(k,0) > _ch_vec(0))
          _pos_a(k,0) -= _ch_vec(0);
				if(_pos_a(k,0) < 0.0)
          _pos_a(k,0) += _ch_vec(0);
				if(_pos_a(k,1) > _ch_vec(1))
          _pos_a(k,1) -= _ch_vec(1);
				if(_pos_a(k,1) < 0.0)
          _pos_a(k,1) += _ch_vec(1);

				if(_pos_b(k,0) > _ch_vec(0))
          _pos_b(k,0) -= _ch_vec(0);
				if(_pos_b(k,0) < 0.0)
          _pos_b(k,0) += _ch_vec(0);
				if(_pos_b(k,1) > _ch_vec(1))
          _pos_b(k,1) -= _ch_vec(1);
				if(_pos_b(k,1) < 0.0)
          _pos_b(k,1) += _ch_vec(1);

				k++;
			}
		}
	}

  std::cout << "\n...atom coordinates:\n";
  _pos_a.print("pos_a:");
  _pos_b.print("pos_b:");

	if (k != _Nu)
	{
		std::cout << "error in finding position of atoms in cnt unit cell!!!" << std::endl;
		std::cout << "Nu = " << _Nu << "  ,  k = " << k << std::endl;
		exit(1);
	}

	// put position of all atoms in a single variable in 2d space(unrolled graphene sheet)
	_pos_2d = arma::mat(2*_Nu,2,arma::fill::zeros);
  _pos_2d(arma::span(0,_Nu-1),arma::span::all) = _pos_a;
  _pos_2d(arma::span(_Nu,2*_Nu-1),arma::span::all) = _pos_b;

	// calculate position of all atoms in the 3d space (rolled graphene sheet)
	_pos_3d = arma::mat(2*_Nu,3,arma::fill::zeros);
	for (int i=0; i<_pos_3d.n_rows; i++)
	{
		_pos_3d(i,0) = _radius*cos(_pos_2d(i,0)/_radius);
		_pos_3d(i,1) = _pos_2d(i,1);
		_pos_3d(i,2) = _radius*sin(_pos_2d(i,0)/_radius);
	}

	// save coordinates of atoms in 2d space
  std::string filename = _directory.path().string() + _name + ".pos_2d.dat";
  _pos_2d.save(filename, arma::arma_ascii);

  // save coordinates of atoms in 3d space
  filename = _directory.path().string() + _name + ".pos_3d.dat";
  _pos_3d.save(filename, arma::arma_ascii);

};

// calculate electron dispersion energies using full unit cell (2*Nu atoms)
void cnt::electron_full()
{

	// make the list of 1st nearest neighbor atoms
	arma::umat nn_list(2*_Nu,3,arma::fill::zeros); // contains index of the nearest neighbor atom
	arma::imat nn_tvec_index(2*_Nu,3,arma::fill::zeros); // contains the index of the cnt unit cell that the nearest neigbor atom is in.
	for (int i=0; i<_pos_3d.n_rows; i++)
	{
		int k=0;
		for (int j=0; j<_pos_3d.n_rows; j++)
		{
			for (int l=-1; l<=1; l++)
			{
        double dR = arma::norm(_pos_3d.row(i)-_pos_3d.row(j)+double(l)*_t_vec_3d.t());
				if ( (i!=j) && (dR<(1.4*_a_cc)) )
				{
					nn_list(i,k) = j;
					nn_tvec_index(i,k) = l;
					k++;
				}
			}
		}
    if (k != 3)
    {
      std::cout << "error: nearest neighbors partially found!!!\n";
      std::exit(1);
    }
	}

	arma::cx_mat H(2*_Nu, 2*_Nu, arma::fill::zeros);
	arma::cx_mat S(2*_Nu, 2*_Nu, arma::fill::zeros);
  arma::vec E;
  arma::cx_mat C;

	int NK = _nk_K1;

	_el_energy_full = arma::mat(2*_Nu, NK, arma::fill::zeros);
	_el_psi_full = arma::cx_cube(2*_Nu, 2*_Nu, NK, arma::fill::zeros);

	double t_len = arma::norm(_t_vec_3d);

	for (int n=0; n<NK; n++)
	{
		double wave_vec = double(n-_nk_K1/2)*arma::norm(_dk_l);

		H.zeros();
		S.zeros();

		for (int i=0; i<2*_Nu; i++)
		{
			H(i,i) = (_e2p,0.e0);
			for (int k=0; k<3; k++)
			{
				int j = nn_list(i,k);
				int l = nn_tvec_index(i,k);

				H(i,j) += arma::cx_double(_t0,0.e0)*exp(arma::cx_double(0.0,wave_vec*double(l)*t_len));
				S(i,j) += arma::cx_double(_s0,0.e0)*exp(arma::cx_double(0.0,wave_vec*double(l)*t_len));
			}
		}

		arma::eig_sym(E, C, H);

		// fix the phase of the eigen vectors
		for (int i=0; i<C.n_cols; i++)
		{
			arma::cx_double phi = std::conj(C(0,i))/std::abs(C(0,i));
			for (int j=0; j<C.n_rows; j++)
			{
				C(j,i) *= phi;
			}
		}

    _el_energy_full.col(n) = E;
    _el_psi_full.slice(n) = C;

	}

  // save electron energy bands using full Brillouine zone
  std::string filename = _directory.path().string() + _name + ".el_energy_full.dat";
  _el_energy_full.save(filename, arma::arma_ascii);

  // // save electron wavefunctions using full Brillouine zone
  // filename = _directory.path().string() + _name + ".el_psi_full.dat";
  // _el_psi_full.save(filename, arma::arma_ascii);

};

// calculate electron dispersion energies using the K1-extended representation
void cnt::electron_K1_extended()
{
  int number_of_bands = 2;
  int number_of_atoms_in_graphene_unit_cell = number_of_bands;

  _el_energy_redu = arma::cube(number_of_bands, _nk_K1, _Nu, arma::fill::zeros);
  _el_psi_redu = arma::field<arma::cx_cube>(_Nu); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
  _el_psi_redu.for_each([&](arma::cx_cube& c){c.zeros(number_of_atoms_in_graphene_unit_cell, number_of_bands, _nk_K1);}); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently

  const std::complex<double> i1(0,1);

  for (int mu=0; mu<_Nu; mu++)
  {
    for (int ik=0; ik<_nk_K1; ik++)
    {
      arma::vec k_vec = mu*_K1 + ik*_dk_l;
      std::complex<double> fk = std::exp(std::complex<double>(0,arma::dot(k_vec,(_a1+_a2)/3.))) + std::exp(std::complex<double>(0,arma::dot(k_vec,(_a1-2.*_a2)/3.))) + std::exp(std::complex<double>(0,arma::dot(k_vec,(_a2-2.*_a1)/3.)));
      const int ic = 0;
      const int iv = 1;
      _el_energy_redu(ic,ik,mu) = -_t0*std::abs(fk);
      _el_energy_redu(iv,ik,mu) = +_t0*std::abs(fk);

      const int iA = 0;
      const int iB = 1;
      (_el_psi_redu(mu))(iA,ic,ik) = +1./std::sqrt(2.); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_redu(mu))(iA,iv,ik) = +1./std::sqrt(2.); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_redu(mu))(iB,ic,ik) = +1./std::sqrt(2.)*std::conj(fk)/std::abs(fk); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_redu(mu))(iB,iv,ik) = -1./std::sqrt(2.)*std::conj(fk)/std::abs(fk); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
    }
  }

  // save electron energy bands using full Brillouine zone
  std::string filename = _directory.path().string() + _name + ".el_energy_redu.dat";
  _el_energy_redu.save(filename,arma::arma_ascii);

  // find the mu value for bands with extremums

};

// calculate electron dispersion energies using the K2-extended representation
void cnt::electron_K2_extended()
{
  int number_of_bands = 2;
  int number_of_atoms_in_graphene_unit_cell = number_of_bands;

  _ik_min_K2 = 0;
  _ik_max_K2 = _Nu/_Q*_nk_K1;
  _nk_K2 = _ik_max_K2 - _ik_min_K2;

  _mu_min_K2 = 0;
  _mu_max_K2 = _Q;
  _n_mu_K2 = _mu_max_K2 - _mu_min_K2;


  _el_energy_K2 = arma::cube(number_of_bands, _nk_K2, _n_mu_K2, arma::fill::zeros);
  _el_psi_K2 = arma::field<arma::cx_cube>(_n_mu_K2); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
  _el_psi_K2.for_each([&](arma::cx_cube& c){c.zeros(number_of_atoms_in_graphene_unit_cell, number_of_bands, _nk_K2);}); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently

  const std::complex<double> i1(0,1);

  for (int mu=_mu_min_K2; mu<_mu_max_K2; mu++)
  {
    for (int ik=_ik_min_K2; ik<_ik_max_K2; ik++)
    {
      arma::vec k_vec = double(mu)*_K1 + double(ik)*_dk_l;
      std::complex<double> fk = std::exp(std::complex<double>(0,arma::dot(k_vec,(_a1+_a2)/3.))) + std::exp(std::complex<double>(0,arma::dot(k_vec,(_a1-2.*_a2)/3.))) + std::exp(std::complex<double>(0,arma::dot(k_vec,(_a2-2.*_a1)/3.)));
      const int ic = 1;
      const int iv = 0;
      _el_energy_K2(ic,ik-_ik_min_K2,mu-_mu_min_K2) = +_t0*std::abs(fk);
      _el_energy_K2(iv,ik-_ik_min_K2,mu-_mu_min_K2) = -_t0*std::abs(fk);

      const int iA = 0;
      const int iB = 1;
      (_el_psi_K2(mu-_mu_min_K2))(iA,ic,ik-_ik_min_K2) = +1./std::sqrt(2.); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_K2(mu-_mu_min_K2))(iA,iv,ik-_ik_min_K2) = +1./std::sqrt(2.); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_K2(mu-_mu_min_K2))(iB,ic,ik-_ik_min_K2) = -1./std::sqrt(2.)*std::conj(fk)/std::abs(fk); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
      (_el_psi_K2(mu-_mu_min_K2))(iB,iv,ik-_ik_min_K2) = +1./std::sqrt(2.)*std::conj(fk)/std::abs(fk); // this pretty weird order is chosen so than we can select each cutting line easier and more efficiently
    }
  }

  // save electron energy bands using full Brillouine zone
  std::string filename = _directory.path().string() + _name + ".el_energy_K2.dat";
  _el_energy_K2.save(filename,arma::arma_ascii);

  std::cout << "\n...calculated K2-extended electron dispersion\n";
};

void cnt::find_K2_extended_valleys()
{


  // get the indicies of valleys
  std::vector<std::array<unsigned int,2>> ik_valley_idx;

  int iC = 1;
  for (int ik_idx=0; ik_idx<_el_energy_K2.n_cols; ik_idx++)
  {
    for (int i_mu_idx=0; i_mu_idx<_el_energy_K2.n_slices; i_mu_idx++)
    {
      int ik_idx_p1 = ik_idx + 1;
      int ik_idx_m1 = ik_idx - 1;
      while(ik_idx_p1 >= _el_energy_K2.n_cols)
      {
        ik_idx_p1 -= _el_energy_K2.n_cols;
      }
      while(ik_idx_m1 < 0)
      {
        ik_idx_m1 += _el_energy_K2.n_cols;
      }

      if ((_el_energy_K2(iC,ik_idx,i_mu_idx) < _el_energy_K2(iC,ik_idx_m1,i_mu_idx)) \
            and (_el_energy_K2(iC,ik_idx,i_mu_idx) < _el_energy_K2(iC,ik_idx_p1,i_mu_idx)))
      {
        // arma::umat tmp = {{(unsigned int)ik_idx, (unsigned int)i_mu_idx}};
        // ik_valley_idx.insert_rows(0,tmp);
        ik_valley_idx.push_back({(unsigned int)ik_idx, (unsigned int)i_mu_idx});
      }

    }
  }

  // sort valleys in order of their 
  std::sort(ik_valley_idx.begin(),ik_valley_idx.end(), [&](const auto& s1, const auto& s2) {
                                    return _el_energy_K2(1,s1[0],s1[1]) < _el_energy_K2(1,s2[0],s2[1]);
                                  });

  // put them in a vector with each element containing the two equivalent valleys
  for (int i=0; i<ik_valley_idx.size()/2; i++)
  {
    std::array<std::array<unsigned int, 2>, 2> valley = {ik_valley_idx.at(2*i), ik_valley_idx.at(2*i+1)};
    _valleys_K2.push_back(valley);
  }

  std::cout << "\n...found and sorted indices of valleys:\n";
  for (const auto& valleys: _valleys_K2)
  {
    auto v1 = valleys.at(0);
    auto v2 = valleys.at(1);
    std::cout << "[" << v1[0] << "," << v1[1] << "] , [" << v2[0] << "," << v2[1] << "]\n";
  }

  std::cout << "number of valleys: " << _valleys_K2.size() << std::endl;
  
};


// find ik values that are energetically relevant around the bottom of the valley
void cnt::find_relev_ik_range(double delta_energy)
{
  std::vector<std::vector<std::array<int,2>>> relev_ik_range(2);

  int nk_K2 = _ik_max_K2 - _ik_min_K2;
  
  // first get ik for relevant states in the first valley
  int i_valley = 0;
  int ik_bottom = _valleys_K2[_i_sub][i_valley][0]+_ik_min_K2;
  int mu_bottom = _valleys_K2[_i_sub][i_valley][1]+_mu_min_K2;
  int iC = 1;
  double max_energy = _el_energy_K2(iC,ik_bottom-_ik_min_K2,mu_bottom-_mu_min_K2)+delta_energy;
  // std::cout << "max_energy: " << max_energy/constants::eV << std::endl;
  // std::cout << "delta_energy: " << delta_energy/constants::eV << std::endl;
  // std::cout << "_el_energy_K2: " << _el_energy_K2(iC,ik_bottom-_ik_min_K2,mu_bottom-_mu_min_K2)/constants::eV << std::endl;
  
  relev_ik_range.at(i_valley).push_back({ik_bottom,mu_bottom});
  bool in_range = true;
  int count = 0;
  while (in_range)
  {
    in_range = false;
    count ++;
    int ik = ik_bottom + count;
    while(ik >= _ik_max_K2)
    {
      ik -= (_ik_max_K2-_ik_min_K2);
    }
    if (_el_energy_K2(iC,ik-_ik_min_K2,mu_bottom-_mu_min_K2) < max_energy)
    {
      relev_ik_range.at(i_valley).push_back({ik,mu_bottom});
      in_range = true;
    }

    ik = ik_bottom - count;
    while(ik < _ik_min_K2)
    {
      ik += (_ik_max_K2-_ik_min_K2);
    }
    if (_el_energy_K2(iC,ik-_ik_min_K2,mu_bottom-_mu_min_K2) < max_energy)
    {
      // std::array<int,2> relev_state = {ik,mu_bottom};
      relev_ik_range.at(i_valley).insert(relev_ik_range.at(i_valley).begin(), {ik,mu_bottom});
      in_range = true;
    }
  }

  // do the same thing for the second valley
  i_valley = 1;
  ik_bottom = _valleys_K2[_i_sub][i_valley][0]+_ik_min_K2;
  mu_bottom = _valleys_K2[_i_sub][i_valley][1]+_mu_min_K2;
  max_energy = _el_energy_K2(iC,ik_bottom-_ik_min_K2,mu_bottom-_mu_min_K2)+delta_energy;
  
  relev_ik_range.at(i_valley).push_back({ik_bottom,mu_bottom});
  in_range = true;
  count = 0;
  while (in_range)
  {
    in_range = false;
    count ++;
    int ik = ik_bottom + count;
    while(ik >= _ik_max_K2)
    {
      ik -= (_ik_max_K2-_ik_min_K2);
    }
    if (_el_energy_K2(iC,ik-_ik_min_K2,mu_bottom-_mu_min_K2) < max_energy)
    {
      relev_ik_range.at(i_valley).push_back({ik,mu_bottom});
      in_range = true;
    }

    ik = ik_bottom - count;
    while(ik < _ik_min_K2)
    {
      ik += (_ik_max_K2-_ik_min_K2);
    }
    if (_el_energy_K2(iC,ik-_ik_min_K2,mu_bottom-_mu_min_K2) < max_energy)
    {
      relev_ik_range.at(i_valley).insert(relev_ik_range.at(i_valley).begin(), {ik,mu_bottom});
      in_range = true;
    }
  }


  std::cout << "\n...ik for relevant states calculated:\n";
  std::cout << "relev_ik_range has length of " << relev_ik_range[0].size() << std::endl;
  
  // i_valley = 0;
  // std::cout << "valley: " << i_valley << "\n";
  // for (const auto& state: relev_ik_range.at(i_valley))
  // {
  //   std::cout << "   [" << state.at(0) << "," << state.at(1) << "]\n";
  // }

  // i_valley = 1;
  // std::cout << "\nvalley: " << i_valley << "\n";
  // for (const auto& state: relev_ik_range.at(i_valley))
  // {
  //   std::cout << "   [" << state.at(0) << "," << state.at(1) << "]\n";
  // }

  _relev_ik_range = relev_ik_range;

};

// fourier transformation of the coulomb interaction a.k.a v(q)
cnt::vq_struct cnt::calculate_vq(const std::array<int,2> iq_range, const std::array<int,2> mu_range, unsigned int no_of_cnt_unit_cells)
{
  // primary checks for function input
  int nq = iq_range.at(1) - iq_range.at(0);
  if (nq <= 0) {
    throw "Incorrect range for iq!";
  }
  int n_mu = mu_range.at(1) - mu_range.at(0);
  if (n_mu <= 0) {
    throw "Incorrect range for mu_q!";
  }
  if (no_of_cnt_unit_cells % 2 == 0)  no_of_cnt_unit_cells ++;

  // calculate distances between atoms in a warped cnt unit cell.
	arma::mat pos_aa = arma::mat(_Nu,2,arma::fill::zeros);
	arma::mat pos_ab = arma::mat(_Nu,2,arma::fill::zeros);
	arma::mat pos_ba = arma::mat(_Nu,2,arma::fill::zeros);
  arma::mat pos_bb = arma::mat(_Nu,2,arma::fill::zeros);

	for (int i=0; i<_Nu; i++)
	{
    pos_aa.row(i) = _pos_a.row(i)-_pos_a.row(0);
    pos_ab.row(i) = _pos_a.row(i)-_pos_b.row(0);
    pos_ba.row(i) = _pos_b.row(i)-_pos_a.row(0);
    pos_bb.row(i) = _pos_b.row(i)-_pos_b.row(0);

		if(pos_aa(i,0) > _ch_vec(0)/2)
      pos_aa(i,0) -= _ch_vec(0);
		if(pos_ab(i,0) > _ch_vec(0)/2)
      pos_ab(i,0) -= _ch_vec(0);
		if(pos_ba(i,0) > _ch_vec(0)/2)
      pos_ba(i,0) -= _ch_vec(0);
		if(pos_bb(i,0) > _ch_vec(0)/2)
      pos_bb(i,0) -= _ch_vec(0);
	}

  arma::cube rel_pos(_Nu*no_of_cnt_unit_cells,2,4,arma::fill::zeros);
  for (int i=-std::floor(double(no_of_cnt_unit_cells)/2.); i<=std::floor(double(no_of_cnt_unit_cells)/2.); i++)
  {
    int idx = (i+std::floor(double(no_of_cnt_unit_cells)/2.))*_Nu;
    for (int j=0; j<_Nu; j++)
    {
      rel_pos.slice(0).row(idx+j) = pos_aa.row(j)+(i*_t_vec.t());
      rel_pos.slice(1).row(idx+j) = pos_ab.row(j)+(i*_t_vec.t());
      rel_pos.slice(2).row(idx+j) = pos_ba.row(j)+(i*_t_vec.t());
      rel_pos.slice(3).row(idx+j) = pos_bb.row(j)+(i*_t_vec.t());
    }
  }

  // calculate vq
  arma::cx_cube vq(nq,n_mu,4,arma::fill::zeros);
  arma::vec q_vec(nq,arma::fill::zeros);

  arma::vec q(2,arma::fill::zeros);
  const double coeff = std::pow(4.*constants::pi*constants::eps0*_Upp/constants::q0/constants::q0,2);
  const std::complex<double> i1(0.,1.);
  auto Uhno = [&](const arma::mat& R){
    return std::exp(i1*arma::dot(q,R))*_Upp/std::sqrt(coeff*(std::pow(R(0),2)+std::pow(R(1),2))+1);
  };

  progress_bar prog;

  for (int iq=iq_range[0]; iq<iq_range[1]; iq++)
  {
    int iq_idx = iq-iq_range[0];
    
    prog.step(iq_idx, nq, "vq",5);

    q_vec(iq_idx) = iq*arma::norm(_dk_l,2);
    for (int mu=mu_range[0]; mu<mu_range[1]; mu++)
    {
      int mu_idx = mu - mu_range[0];
      q = iq*_dk_l + mu*_K1;
      // std::cout << "after addition!\n";
      for (int i=0; i<4; i++)
      {
        for (int k=0; k<_Nu*no_of_cnt_unit_cells; k++)
        {
          vq(iq_idx,mu_idx,i) += Uhno(rel_pos.slice(i).row(k));
        }
      }
    }
  }

  vq = vq/(2*_Nu*no_of_cnt_unit_cells);

  std::cout << "\n...calculated vq\n";

  std::cout << "saved real part of vq\n";
  arma::cube vq_real = arma::real(vq);
  std::string filename = _directory.path().string() + _name + ".vq_real.dat";
  vq_real.save(filename, arma::arma_ascii);

  std::cout << "saved imaginary part of vq\n";
  arma::cube vq_imag = arma::imag(vq);
  filename = _directory.path().string() + _name + ".vq_imag.dat";
  vq_imag.save(filename, arma::arma_ascii);

  std::cout << "saved q_vector for vq\n";
  filename = _directory.path().string() + _name + ".vq_q_vec.dat";
  q_vec.save(filename, arma::arma_ascii);

  // make the vq_struct that is to be returned
  vq_struct vq_s;
  vq_s.data = vq;
  vq_s.iq_range = iq_range;
  vq_s.mu_range = mu_range;
  vq_s.nq = nq;
  vq_s.n_mu = n_mu;

  return vq_s;
};

// polarization of electronic states a.k.a PI(q)
cnt::PI_struct cnt::calculate_polarization(const std::array<int,2> iq_range, const std::array<int,2> mu_range)
{
  // primary checks for function input
  int nq = iq_range.at(1) - iq_range.at(0);
  if (nq <= 0) {
    throw "Incorrect range for iq in calculate_polarization!";
  }
  int n_mu = mu_range.at(1) - mu_range.at(0);
  if (n_mu <= 0) {
    throw "Incorrect range for mu_q in calculate_polarization!";
  }

  // lambda function to wrap iq+ik and mu_k+mu_q inside the K2-extended brillouine zone
  int ikq, mu_kq;
  int ik, mu_k;
  int iq, mu_q;
  auto get_kq = [&](){
    mu_kq = mu_k+mu_q;
    ikq = ik+iq;
    while (mu_kq >= _mu_max_K2) {
      mu_kq -= _n_mu_K2;
      ikq += _nk_K1*_M;
    }
    while (mu_kq < _mu_min_K2) {
      mu_kq += _n_mu_K2;
      ikq -= _nk_K1*_M;
    }
    while (ikq >= _ik_max_K2){
      ikq -= _nk_K2;
    }
    while (ikq < _ik_min_K2){
      ikq += _nk_K2;
    }
  };

  arma::mat PI(nq,n_mu,arma::fill::zeros);
  arma::vec q_vec(nq,arma::fill::zeros);

  const int iv = 0;
  const int ic = 1;
  const int iA = 0;
  const int iB = 1;

  progress_bar prog;

  int iq_idx, mu_q_idx;
  int ik_idx, mu_k_idx;
  int i_kq_idx, mu_kq_idx;
  for (iq=iq_range[0]; iq<iq_range[1]; iq++)
  {
    iq_idx = iq - iq_range[0];
    q_vec(iq_idx) = iq*arma::norm(_dk_l);

    prog.step(iq_idx, nq, "polarization", 5);

    for (mu_q=mu_range[0]; mu_q<mu_range[1]; mu_q++)
    {
      mu_q_idx = mu_q - mu_range[0];
      for (ik=_ik_min_K2; ik<_ik_max_K2; ik++)
      {
        ik_idx = ik - _ik_min_K2;
        for (mu_k=_mu_min_K2; mu_k<_mu_max_K2; mu_k++)
        {
          mu_k_idx = mu_k - _mu_min_K2;
          get_kq();
          mu_kq_idx = mu_kq - _mu_min_K2;
          i_kq_idx = ikq - _ik_min_K2;

          PI(iq_idx,mu_q_idx) += std::pow(std::abs(arma::dot(arma::conj(_el_psi_K2(mu_k_idx).slice(ik_idx).col(iv)),_el_psi_K2(mu_kq_idx).slice(i_kq_idx).col(ic))),2)/ \
                                 (_el_energy_K2(ic,i_kq_idx,mu_kq_idx)-_el_energy_K2(iv,ik_idx,mu_k_idx)) + \
                                 std::pow(std::abs(arma::dot(arma::conj(_el_psi_K2(mu_k_idx).slice(ik_idx).col(ic)),_el_psi_K2(mu_kq_idx).slice(i_kq_idx).col(iv))),2)/ \
                                 (_el_energy_K2(ic,ik_idx,mu_k_idx)-_el_energy_K2(iv,i_kq_idx,mu_kq_idx));
        }
      }
    }
  }

  PI = 2*PI;

  std::cout << "\n...calculated polarization: PI(q)\n";

  std::cout << "saved PI\n";
  std::string filename = _directory.path().string() + _name + ".PI.dat";
  PI.save(filename, arma::arma_ascii);

  std::cout << "saved q_vector for PI\n";
  filename = _directory.path().string() + _name + ".PI_q_vec.dat";
  q_vec.save(filename, arma::arma_ascii);

  // make the vq_struct that is to be returned
  PI_struct PI_s;
  PI_s.data = PI;
  PI_s.iq_range = iq_range;
  PI_s.mu_range = mu_range;
  PI_s.nq = nq;
  PI_s.n_mu = n_mu;

  return PI_s;
};

// dielectric function a.k.a eps(q)
cnt::epsilon_struct cnt::calculate_dielectric(const std::array<int,2> iq_range, const std::array<int,2> mu_range)
{
  // check if vq has been calculated properly before
  if (not (in_range(iq_range,_vq.iq_range) and in_range(mu_range,_vq.mu_range))){
    throw std::logic_error("You need to calculate vq with correct range before \
                            trying to calculate dielectric function");
  }
  // check if PI has been calculated properly before
  if (not (in_range(iq_range,_PI.iq_range) and in_range(mu_range,_PI.mu_range))){
    throw std::logic_error("You need to calculate PI with correct range before \
                            trying to calculate dielectric function");
  }

  int nq = iq_range[1] - iq_range[0];
  int n_mu = mu_range[1] - mu_range[0];
  arma::mat eps = arma::real(arma::mean(_vq.data,2));
  eps = eps.submat(iq_range[0]-_vq.iq_range[0],mu_range[0]-_vq.mu_range[0],arma::size(nq,n_mu));
  eps %= _PI.data.submat(iq_range[0]-_PI.iq_range[0],mu_range[0]-_PI.mu_range[0],arma::size(nq,n_mu));
  std::cout << "size of dielectric function matrix: " << arma::size(eps) << std::endl;
  eps += 1.;

  arma::vec q_vec(nq);
  for (int iq=iq_range[0]; iq<iq_range[1]; iq++)
  {
    int iq_idx = iq - iq_range[0];
    q_vec(iq_idx) = iq*arma::norm(_dk_l);
  }

  std::cout << "\n...calculated dielectric function: epsilon(q)\n";

  std::cout << "saved epsilon\n";
  std::string filename = _directory.path().string() + _name + ".eps.dat";
  eps.save(filename, arma::arma_ascii);

  std::cout << "saved q_vector for epsilon\n";
  filename = _directory.path().string() + _name + ".eps_q_vec.dat";
  q_vec.save(filename, arma::arma_ascii);

  epsilon_struct eps_s;
  eps_s.data = eps;
  eps_s.iq_range = iq_range;
  eps_s.mu_range = mu_range;
  eps_s.nq = nq;
  eps_s.n_mu = n_mu;
  return eps_s;
};

// calculate exciton dispersion
void cnt::calculate_exciton_energy(const std::array<int,2> ik_cm_range)
{
  // some utility variables that are going to be used over and over again
  int ik_c, mu_c;
  int ik_v, mu_v;
  int ik_cp, mu_cp;
  int ik_vp, mu_vp;
  int ik_c_diff, mu_c_diff;
  int ik_cm, mu_cm;

  std::complex<double> dir_interaction;
  std::complex<double> xch_interaction;

  const int iv = 0;
  const int ic = 1;

  const int i_valley_1 = 0;
  const int i_valley_2 = 1;

  // lambda function to calculate direct interaction
  auto get_direct_interaction = [&](){
    ik_c_diff = ik_c-ik_cp;
    mu_c_diff = mu_c-mu_cp;
    while(ik_c_diff < _ik_min_K2){
      ik_c_diff += _nk_K2;
    }
    while(ik_c_diff >= _ik_max_K2){
      ik_c_diff -= _nk_K2;
    }

    dir_interaction = 0;
    for (int i=0; i<2; i++)
    {
      for (int j=0; j<2; j++) 
      {
        dir_interaction += std::conj(_el_psi_K2(mu_c -_mu_min_K2)(i,ic,ik_c -_ik_min_K2))* \
                                     _el_psi_K2(mu_v -_mu_min_K2)(j,iv,ik_v -_ik_min_K2) * \
                                     _el_psi_K2(mu_cp-_mu_min_K2)(i,ic,ik_cp-_ik_min_K2) * \
                           std::conj(_el_psi_K2(mu_vp-_mu_min_K2)(j,iv,ik_vp-_ik_min_K2))* \
                      _vq.data(ik_c_diff-_vq.iq_range[0],mu_c_diff-_vq.mu_range[0],2*i+j)/ \
                      _eps.data(ik_c_diff-_eps.iq_range[0],mu_c_diff-_eps.mu_range[0]);
      }
    }
    return dir_interaction;
  };

  // lambda function to calculate exchange interaction
  auto get_exchange_interaction = [&](){
    xch_interaction = 0;
    for (int i=0; i<2; i++)
    {
      for (int j=0; j<2; j++) 
      {
        xch_interaction += std::conj(_el_psi_K2(mu_c -_mu_min_K2)(i,ic,ik_c -_ik_min_K2))* \
                                     _el_psi_K2(mu_v -_mu_min_K2)(i,iv,ik_v -_ik_min_K2) * \
                                     _el_psi_K2(mu_cp-_mu_min_K2)(j,ic,ik_cp-_ik_min_K2) * \
                           std::conj(_el_psi_K2(mu_vp-_mu_min_K2)(j,iv,ik_vp-_ik_min_K2))* \
                              _vq.data(ik_cm-_vq.iq_range[0],mu_cm-_vq.mu_range[0],2*i+j);
      }
    }
    return xch_interaction;
  };


  progress_bar prog;

  int nk_cm = ik_cm_range[1] - ik_cm_range[0];
  int nk_relev = int(_relev_ik_range[0].size());

  arma::mat ex_energy_A1(nk_cm,nk_relev,arma::fill::zeros);
  arma::mat ex_energy_A2_singlet(nk_cm,nk_relev,arma::fill::zeros);
  arma::mat ex_energy_A2_triplet(nk_cm,nk_relev,arma::fill::zeros);

  arma::cx_mat kernel_11(nk_relev,nk_relev,arma::fill::zeros);
  arma::cx_mat kernel_12(nk_relev,nk_relev,arma::fill::zeros);
  arma::cx_mat kernel_exchange(nk_relev,nk_relev,arma::fill::zeros);
  arma::vec energy;
  arma::vec k_cm_vec(nk_cm,arma::fill::zeros);

  // loop to calculate exciton dispersion
  for (ik_cm=ik_cm_range[0]; ik_cm<ik_cm_range[1]; ik_cm++)
  {
    kernel_11.zeros();
    kernel_12.zeros();
    kernel_exchange.zeros();
    mu_cm = 0;
    int ik_cm_idx = ik_cm - ik_cm_range[0];
    k_cm_vec(ik_cm_idx) = ik_cm*arma::norm(_dk_l);

    prog.step(ik_cm_idx, nk_cm, "ex_energy", 5);

    
    for (int ik_c_idx=0; ik_c_idx<nk_relev; ik_c_idx++)
    {
      ik_c = _relev_ik_range[i_valley_1][ik_c_idx][0];
      mu_c = _relev_ik_range[i_valley_1][ik_c_idx][1];
      ik_v = get_ikv(ik_c,ik_cm);
      mu_v = mu_c;

      kernel_11(ik_c_idx, ik_c_idx) += _el_energy_K2(ic,ik_c-_ik_min_K2,mu_c-_mu_min_K2) - _el_energy_K2(iv,ik_v-_ik_min_K2,mu_c-_mu_min_K2);

      // interaction  between valley_1 and valley_1
      for (int ik_cp_idx=0; ik_cp_idx<=ik_c_idx; ik_cp_idx++)
      {
        ik_cp = _relev_ik_range[i_valley_1][ik_cp_idx][0];
        mu_cp = _relev_ik_range[i_valley_1][ik_cp_idx][1];
        ik_vp = get_ikv(ik_cp,ik_cm);
        mu_vp = mu_cp;

        kernel_11(ik_c_idx,ik_cp_idx) -= get_direct_interaction();
        kernel_exchange(ik_c_idx,ik_cp_idx) += std::complex<double>(2,0)*get_exchange_interaction();
      }

      // interaction  between valley_1 and valley_2
      for (int ik_vp_idx=ik_c_idx; ik_vp_idx<nk_relev; ik_vp_idx++)
      {
        ik_vp = _relev_ik_range[i_valley_2][ik_vp_idx][0];
        mu_vp = _relev_ik_range[i_valley_2][ik_vp_idx][1];
        ik_cp = get_ikc(ik_vp,ik_cm);
        mu_cp = mu_vp;

        kernel_12(ik_c_idx,nk_relev-1-ik_vp_idx) -= get_direct_interaction();
      }
    }


    kernel_11 += kernel_11.t();
    kernel_12 += kernel_12.t();
    kernel_exchange += kernel_exchange.t();
    for (int ik_c_idx=0; ik_c_idx<nk_relev; ik_c_idx++)
    {
      kernel_11(ik_c_idx,ik_c_idx) /= std::complex<double>(2,0);
      kernel_12(ik_c_idx,ik_c_idx) /= std::complex<double>(2,0);
      kernel_exchange(ik_c_idx,ik_c_idx) /= std::complex<double>(2,0);
    }

    energy = arma::eig_sym(kernel_11-kernel_12);
    ex_energy_A1.row(ik_cm_idx) = energy.t();

    energy = arma::eig_sym(kernel_11+kernel_12);
    ex_energy_A2_triplet.row(ik_cm_idx) = energy.t();
    
    // kernel += kernel_exchange;
    energy = arma::eig_sym(kernel_11+kernel_12+std::complex<double>(2,0)*kernel_exchange);
    ex_energy_A2_singlet.row(ik_cm_idx) = energy.t();
  }

  std::cout << "\n...calculated exciton dispersion\n";

  std::cout << "saved exciton dispersion: A2 singlet\n";
  std::string filename = _directory.path().string() + _name + ".ex_energy_A2_singlet.dat";
  ex_energy_A2_singlet.save(filename, arma::arma_ascii);

  std::cout << "saved exciton dispersion: A2 triplet\n";
  filename = _directory.path().string() + _name + ".ex_energy_A2_triplet.dat";
  ex_energy_A2_triplet.save(filename, arma::arma_ascii);

  std::cout << "saved exciton dispersion: A1\n";
  filename = _directory.path().string() + _name + ".ex_energy_A1.dat";
  ex_energy_A1.save(filename, arma::arma_ascii);

  std::cout << "saved k_vector for center of mass\n";
  filename = _directory.path().string() + _name + ".exciton_k_cm_vec.dat";
  k_cm_vec.save(filename, arma::arma_ascii);

};

// call this to do all the calculations at once
void cnt::calculate_exciton_dispersion()
{
  get_parameters();
  get_atom_coordinates();
  electron_K2_extended();
  find_K2_extended_valleys();
  find_relev_ik_range(1.*constants::eV);

  std::array<int,2> iq_range = {-(_ik_max_K2-1),_ik_max_K2};
  std::array<int,2> mu_range = {-(_Q-1),_Q};
  _vq = calculate_vq(iq_range, mu_range, _number_of_cnt_unit_cells);
  _PI = calculate_polarization(iq_range, mu_range);
  _eps = calculate_dielectric(iq_range, mu_range);
  
  std::array<int,2> ik_cm_range = {-int(_relev_ik_range[0].size()), int(_relev_ik_range[0].size())};
  calculate_exciton_energy(ik_cm_range);
};