//! [snippet1]

#include <kmer_quasi_indexer.hpp>

using namespace std;

/********************************************************************************/

// We define some constant strings for names of command line parameters
//static const char* STR_FOO = "-foo";
static const char* STR_URI_BANK_INPUT = "-bank";
static const char* STR_URI_QUERY_INPUT = "-query";
static const char* STR_FINGERPRINT = "-fingerprint_size";
static const char* STR_THRESHOLD = "-kmer_threshold";
static const char* STR_OUT_FILE = "-out";

/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  :
 ** REMARKS :
 *********************************************************************/
kmer_quasi_indexer::kmer_quasi_indexer ()  : Tool ("kmer_quasi_indexer")
{
	// We add some custom arguments for command line interface
	getParser()->push_back (new OptionOneParam (STR_URI_GRAPH, "graph input",   true));
	//    getParser()->push_back (new OptionOneParam (STR_VERBOSE,   "verbosity (0:no display, 1: display kmers, 2: display distrib",  false, "0"));
	getParser()->push_back (new OptionOneParam (STR_URI_BANK_INPUT, "bank input",    true));
	getParser()->push_back (new OptionOneParam (STR_URI_QUERY_INPUT, "query input",    true));
	getParser()->push_back (new OptionOneParam (STR_OUT_FILE, "output_file",    true));
	getParser()->push_back (new OptionOneParam (STR_THRESHOLD, "Minimal number of shared kmers for considering 2 reads as similar",    false, "10"));
	getParser()->push_back (new OptionOneParam (STR_FINGERPRINT, "fingerprint size",    false, "8"));


}



/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  :
 ** REMARKS :
 *********************************************************************/
void kmer_quasi_indexer::create_quasi_dictionary (int fingerprint_size){
	const int display = getInput()->getInt (STR_VERBOSE);
	// We get a handle on the HDF5 storage object.
	// Note that we use an auto pointer since the StorageFactory dynamically allocates an instance
	auto_ptr<Storage> storage (StorageFactory(STORAGE_HDF5).load (getInput()->getStr(STR_URI_GRAPH)));
	// We get the group for dsk
	Group& dskGroup = storage->getGroup("dsk");
	kmer_size = atoi(dskGroup.getProperty("kmer_size").c_str());

	// We get the solid kmers collection 1) from the 'dsk' group  2) from the 'solid' collection
	Partition<Kmer<>::Count>& solidKmers = dskGroup.getPartition<Kmer<>::Count> ("solid");


	/** We get the number of solid kmers. */
	const u_int64_t solidFileSize = solidKmers.getNbItems();


	nbSolidKmers = solidKmers.getNbItems();



	IteratorKmerH5Wrapper iteratorOnKmers (solidKmers.iterator());

	//////////////// CREATION QUASI_DICTIONARY
	quasiDico = quasiDictionnaryKeyGeneric<IteratorKmerH5Wrapper> (nbSolidKmers, iteratorOnKmers, fingerprint_size);

	cout<<"Allocating space for read ids"<<endl;
	// We create an iterator for our [kmer,abundance] values
	ProgressIterator<Kmer<>::Count> iter (solidKmers);
	// We iterate the solid kmers from the retrieved collection
	for (iter.first(); !iter.isDone(); iter.next())
	{
		// shortcut
		Kmer<>::Count& count = iter.item();
		bool exists;
		u_int64_t index = quasiDico.get_index(count.value.getVal(), exists);
		if (!exists) {
			cerr<<"Impossible : a kmer should have an entry"<<endl;
			exit(1);
		}
		vector<u_int32_t>* ids_array_one_kmer  = new vector<u_int32_t>(count.abundance);
//		u_int32_t* ids_array_one_kmer = (u_int32_t*) calloc(count.abundance+1, sizeof(u_int32_t));
//		if(!ids_array_one_kmer){
//			cerr<<"Cannot allocate memory"<<endl;
//			exit(1);
//		}
//		cout<<"alloc "<<count.abundance<<" for "<<count.value.getVal()<<endl;
//		ids_array_one_kmer[0]=0; // last used position (== number of inserted elements)
//		for(size_t i=0;i<count.abundance;i++) ids_array_one_kmer[i]=0;
		quasiDico.set_value_known_address(index, ids_array_one_kmer);


	}

	// Here the quasi dico contains a vector of nbSolidKmers * (u_int32_t*)
	// Each value is an array of elements. Each element is a kmer and contains c entries, the count associated to the kmer.
	// We allocate memory for each of these elements:

//	for(auto & kmercount=solidKmers.iterator()->first(); kmercount != solidKmers.iterator()->end(); kmercount++){
//	for(auto & kmercount: solidKmers.iterator()){

//	}
}
static int NT2int(char nt)  {  return (nt>>1)&3;  }

/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  : True if the sequence is of high complexity else return false
 ** REMARKS :
 *********************************************************************/
inline bool highComplexity(Sequence& seq){
	const char* data = seq.getDataBuffer();
    int DUSTSCORE[64]; // all tri-nucleotides
    for (int i=0; i<64; i++) DUSTSCORE[i]=0;
    size_t lenseq =seq.getDataSize();

    for (int j=2; j<lenseq; ++j) ++DUSTSCORE[NT2int(data[j-2])*16 + NT2int(data[j-1])*4 + NT2int(data[j])];
    int m,s=0;

    for (int i=0; i<64; ++i)
    {
        m = DUSTSCORE[i];
        s  += (m*(m-1))/2;
    }

    return s<((lenseq-2)/4 * (lenseq-6)/4)/2;
}

inline const bool correctSequence(Sequence& s){

	const char* data = s.getDataBuffer();

    for (size_t i=0; i<s.getDataSize(); i++)
    {
        if (data[i]!='A' && data[i]!='C' && data[i]!='G' && data[i]!='T')  { return false; }
    }
    return true;
}

// We define a functor that will be cloned by the dispatcher
struct FunctorIndexer
{

	ISynchronizer* synchro;
	quasiDictionnaryKeyGeneric <IteratorKmerH5Wrapper> &quasiDico;
	int kmer_size;
	FunctorIndexer (ISynchronizer* synchro, quasiDictionnaryKeyGeneric <IteratorKmerH5Wrapper>& quasiDico, int kmer_size)  : synchro(synchro), quasiDico(quasiDico), kmer_size(kmer_size) {

	}
	void operator() (Sequence& seq)
	{

		if (!correctSequence(seq)){return;}
		if (!highComplexity(seq)){return;} //TODO: a unique function for optimization
		// We declare a canonical model with a given span size.
		Kmer<KMER_SPAN(0)>::ModelCanonical model (kmer_size);
		// We declare an iterator on a given sequence.
		Kmer<KMER_SPAN(0)>::ModelCanonical::Iterator itKmer (model);
		// We create an iterator over this bank.
		// We set the data from which we want to extract kmers.
		itKmer.setData (seq.getData());
		//        cout<<itSeq->getDataBuffer();
		// We iterate the kmers.
		for (itKmer.first(); !itKmer.isDone(); itKmer.next())
		{
			// Adding the read id to the list of ids associated to this kmer.
			// note that the kmer may not exist in the dictionnary if it was under the solidity threshold.
			// in this case, nothing is done
			u_int32_t read_id = static_cast<u_int32_t>(seq.getIndex());
//			synchro->lock();
//			quasiDico.set_value(itKmer->value().getVal(), read_id, synchro);
			// parcours des cases du quasiDico.set_value(itKmer->value().getVal()) et ajout de read_id+1 à la premiere position == 0
			bool exists;
			vector<u_int32_t>* ids_array = (vector<u_int32_t>*) quasiDico.get_value(itKmer->value().getVal(), exists);
			if(!exists){continue;}
			ids_array->push_back(read_id);
//			cout<<"For "<<itKmer->value().getVal()<<endl;
//			cout<<"avt "<<"ids_array["<<ids_array[0]+1<<" ] = "<<ids_array[ids_array[0]+1]<<endl;
//			ids_array[ids_array[0]+1]=read_id+1;
//			cout<<"aps "<<"ids_array["<<ids_array[0]+1<<" ] = "<<ids_array[ids_array[0]+1]<<endl;
//			ids_array[0]++;
//			for(size_t i=1;i<ids_array[0]+1;i++){
//				cout<<"i "<<i<<" "<<ids_array[i]<<endl;
//				if(ids_array[i]) continue; // BUG ICI: si le tableau est plein on tombe chez les voisins; Mais il ne devrait jamais etre plein...
//				ids_array[i]=read_id+1; // + 1 avoids the 0 id value, considered as non informed
//				break;
//			}
//			synchro->unlock();
		}
	}
};
/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  :
 ** REMARKS :
 *********************************************************************/
void kmer_quasi_indexer::fill_quasi_dictionary (const int nbCores){
	bool exists;
	IBank* bank = Bank::open (getInput()->getStr(STR_URI_BANK_INPUT));
	cout<<"Index "<<kmer_size<<"-mers from bank "<<getInput()->getStr(STR_URI_BANK_INPUT)<<endl;
	LOCAL (bank);
	ProgressIterator<Sequence> itSeq (*bank);

	// We need a general synchronization mechanism that will be shared by the threads.
	ISynchronizer* synchro = System::thread().newSynchronizer();


	// We create a dispatcher configured for 'nbCores' cores.
	Dispatcher dispatcher (1, 1000); // TODO: parallelisation sucks.
	// We iterate the range.  NOTE: we could also use lambda expression (easing the code readability)
	dispatcher.iterate (itSeq, FunctorIndexer(synchro,quasiDico, kmer_size));


	delete synchro;
	//
	//    // We loop over sequences.
	//    for (itSeq.first(); !itSeq.isDone(); itSeq.next())
	//    {
	//        // We set the data from which we want to extract kmers.
	//        itKmer.setData (itSeq->getData());
	//        if (containsN(itSeq->getDataBuffer())){continue;}
	////        cout<<itSeq->getDataBuffer();
	//        // We iterate the kmers.
	//        for (itKmer.first(); !itKmer.isDone(); itKmer.next())
	//        {
	//        	// Adding the read id to the list of ids associated to this kmer.
	//        	// note that the kmer may not exist in the dictionnary if it was under the solidity threshold.
	//        	// in this case, nothing is done
	//        	u_int32_t truc = static_cast<u_int32_t>(itSeq->getIndex());
	//            quasiDico.set_value(itKmer->value().getVal(), truc);
	//        }
	//    }
}



// We define a functor that will be cloned by the dispatcher
struct FunctorQuery
{
	ISynchronizer* synchro;
	ofstream& outFile;
	int kmer_size;
	const quasiDictionnaryKeyGeneric <IteratorKmerH5Wrapper > &quasiDico;
	const int threshold;

	FunctorQuery (ISynchronizer* synchro, ofstream& outFile,  const int kmer_size, const quasiDictionnaryKeyGeneric <IteratorKmerH5Wrapper> &quasiDico, const int threshold)  : synchro(synchro), outFile(outFile), kmer_size(kmer_size), quasiDico(quasiDico), threshold(threshold) {

	}
	void operator() (Sequence& seq)
	{

		if (!correctSequence(seq)){// || !highComplexity(seq)){
			synchro->lock ();
			outFile<<seq.getIndex()<<" U"<<endl;
			synchro->unlock ();

			return;}
		// We declare a canonical model with a given span size.
		Kmer<KMER_SPAN(0)>::ModelCanonical model (kmer_size);
		// We declare an iterator on a given sequence.
		Kmer<KMER_SPAN(0)>::ModelCanonical::Iterator itKmer(model);

		bool exists;
		vector<u_int32_t>* associated_read_ids;
//		vector<u_int32_t> similar_read_ids;



		std::unordered_map<u_int32_t, std::pair <int,int>> similar_read_ids_position_count; // each bank read id --> couple<next viable position (without overlap), number of shared kmers>


		// We set the data from which we want to extract kmers.
		itKmer.setData (seq.getData());
		// We iterate the kmers.
		int i=0; // position on the read
		for (itKmer.first(); !itKmer.isDone(); itKmer.next())
		{
			associated_read_ids=(vector<u_int32_t>*)quasiDico.get_value(itKmer->value().getVal(),exists);
			if(!exists) continue;
			for(std::vector<u_int32_t>::iterator it = associated_read_ids->begin(); it != associated_read_ids->end(); ++it) {
				const u_int32_t read_id=*it;
				std::unordered_map<u_int32_t, std::pair <int,int>>::const_iterator element = similar_read_ids_position_count.find(read_id);
				if(element == similar_read_ids_position_count.end()) {// not inserted yet:
					similar_read_ids_position_count[read_id]=std::make_pair(i+kmer_size, 1);
//					cout<<"create a new pair for "<<read_id<<" ("<<(i+kmer_size)<< ", 1)"<<endl;
				}
				else{  // a kmer is already shared with this read
					std::pair <int,int> viablepos_nbshared = (element->second);
//					cout<<"adding position "<<i<<" for kmer "<<read_id<<" ?"<<endl;
//					cout<<i<<" >=? "<<viablepos_nbshared.first<<endl;
					if(i>=viablepos_nbshared.first){ // the current position does not overlap the previous shared kmer
//						cout<<"YES"<<endl;
						viablepos_nbshared.first = i+kmer_size; // next non overlapping position
						viablepos_nbshared.second = viablepos_nbshared.second+1; // a new kmer shared.
						similar_read_ids_position_count[read_id] = viablepos_nbshared;
					}
//					else cout<<"NO"<<endl;
//					cout<<"cupdated pair for "<<read_id<<" ("<<viablepos_nbshared.first<<", "<<viablepos_nbshared.second<< ")"<<endl;

				}
			}

//			similar_read_ids.insert(similar_read_ids.end(),associated_read_ids.begin(),associated_read_ids.end());
			i++;

		}
//		sort(similar_read_ids.begin(),similar_read_ids.end());

		// We lock the synchronizer
		synchro->lock ();
		outFile<<seq.getIndex()<<" ";
		for (auto &matched_read:similar_read_ids_position_count){
			if (std::get<1>(matched_read.second) >threshold) outFile<<"["<<matched_read.first<<" "<<std::get<1>(matched_read.second)<<"] ";
		}
//		u_int32_t prev_id=-1; //TODO: trick for avoiding problem of unsigned value (get first id stored)
//		int nb_shared=0;
//		for (auto &element:similar_read_ids){
//			if(prev_id != element){
//				if(prev_id!=-1){
//					if(nb_shared>threshold)
//						outFile<<"["<<prev_id<<" "<<nb_shared<<"] ";
//				}
//				nb_shared=0;
//				prev_id=element;
//			}
//			nb_shared++;
//		}
		outFile<<endl;
		// We unlock the synchronizer
		synchro->unlock ();
	}
};

/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  :
 ** REMARKS :
 *********************************************************************/
void kmer_quasi_indexer::parse_query_sequences (int threshold, const int nbCores){
	IBank* bank = Bank::open (getInput()->getStr(STR_URI_QUERY_INPUT));
	cout<<"Query "<<kmer_size<<"-mers from bank "<<getInput()->getStr(STR_URI_BANK_INPUT)<<endl;

	ofstream  outFile;
	outFile.open (getInput()->getStr(STR_OUT_FILE));

	outFile << "#query_read_id [target_read_id number_shared_"<<kmer_size<<"mers]* or U (unvalid read, containing not only ACGT characters or low complexity read)"<<endl;


	LOCAL (bank);

	// We create an iterator over this bank.
	ProgressIterator<Sequence> itSeq (*bank);

	// We need a general synchronization mechanism that will be shared by the threads.
	ISynchronizer* synchro = System::thread().newSynchronizer();

	// We create a dispatcher configured for 'nbCores' cores.
	Dispatcher dispatcher (nbCores, 1000);
	// We iterate the range.  NOTE: we could also use lambda expression (easing the code readability)
	dispatcher.iterate (itSeq, FunctorQuery(synchro,outFile, kmer_size, quasiDico, threshold));



	// We loop over sequences.

	//    for (itSeq.first(); !itSeq.isDone(); itSeq.next())
	//    {
	//
	//        if (containsN(itSeq->getDataBuffer())){read_id++; continue;}
	//  	  vector<u_int32_t>similar_read_ids;
	//
	//
	//
	//        // We set the data from which we want to extract kmers.
	//        itKmer.setData (itSeq->getData());
	//        // We iterate the kmers.
	//        for (itKmer.first(); !itKmer.isDone(); itKmer.next())
	//        {
	//        	quasiDico.get_value(itKmer->value().getVal(),exists,associated_read_ids);
	//        	similar_read_ids.insert(similar_read_ids.end(),associated_read_ids.begin(),associated_read_ids.end());
	//
	//        }
	//        sort(similar_read_ids.begin(),similar_read_ids.end());
	//        outFile<<read_id<<" ";
	//        u_int32_t prev_id=-1;
	//        int nb_shared=0;
	//        for (auto &element:similar_read_ids){
	//        	if(prev_id != element){
	//        		if(prev_id!=-1){
	//        			if(nb_shared>threshold)
	//        				outFile<<"["<<prev_id<<" "<<nb_shared<<"] ";
	//        		}
	//        		nb_shared=0;
	//        		prev_id=element;
	//        	}
	//        	nb_shared++;
	//        }
	//        outFile<<endl;
	//
	//        read_id++;
	//    }


	outFile.close();    // We get rid of the synchronizer
	delete synchro;
}

/*********************************************************************
 ** METHOD  :
 ** PURPOSE :
 ** INPUT   :
 ** OUTPUT  :
 ** RETURN  :
 ** REMARKS :
 *********************************************************************/
void kmer_quasi_indexer::execute ()
{
	// We can do here anything we want.
	// For further information about the Tool class, please have a look
	// on the ToyTool snippet  (http://gatb-core.gforge.inria.fr/snippets_tools.html)

	// We gather some statistics.

	int nbCores = 0; //TODO: parameter
	int fingerprint_size = getInput()->getInt(STR_FINGERPRINT);

	// IMPORTANT NOTE:
	// Actually, during the filling of the dictionary values, one may fall on non solid non indexed kmers
	// that are quasi dictionary false positives (ven with a non null fingerprint. This means that one nevers knows in advance how much
	// values are gonna be stored for all kmers. This is why I currently us a vector<u_int32_t> for storing read ids associated to a kmer.

	// We need a non null finger print because of non solid non indexed kmers
//	if (getInput()->getStr(STR_URI_BANK_INPUT).compare(getInput()->getStr(STR_URI_QUERY_INPUT))==0)
//		fingerprint_size=0;

	create_quasi_dictionary(fingerprint_size);
	fill_quasi_dictionary(nbCores);

	int threshold = getInput()->getInt(STR_THRESHOLD);
	parse_query_sequences(threshold-1, nbCores); //-1 avoids >=

	getInfo()->add (1, &LibraryInfo::getInfo());
	getInfo()->add (1, "input");
	getInfo()->add (2, "Reference bank:",  "%s",  getInput()->getStr(STR_URI_BANK_INPUT).c_str());
	getInfo()->add (2, "Query bank:",  "%s",  getInput()->getStr(STR_URI_QUERY_INPUT).c_str());
	getInfo()->add (2, "Kmer Size:", "%d", kmer_size);
	getInfo()->add (2, "Fingerprint size:",  "%d",  fingerprint_size);
	getInfo()->add (2, "Threshold size:",  "%d",  threshold);
	getInfo()->add (1, "output");
	getInfo()->add (2, "Results written in:",  "%s",  getInput()->getStr(STR_OUT_FILE).c_str());




}

