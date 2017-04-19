// [[Rcpp::plugins(cpp11)]]
#include <Rcpp.h>
#include <fstream>
#include <string>
#include <algorithm>

using namespace Rcpp;

typedef std::unordered_map<int, std::unordered_map<int, double>> Int_Int_Double;
typedef std::unordered_map<int, double> Int_Double;
typedef std::unordered_map<int, int> Int_Int;


Int_Int_Double computePosteriors(Int_Int_Double &priorClassFeatureCounts, Int_Double &priorClassCounts, 
                                 Int_Int_Double &rowColValMap, const std::set<int> &classLabels,
                                 const int &numDocs, const int &numFeatures) {
  
  Int_Int_Double posteriorProbs;
  
  Int_Double classFeatureProbsSum;
  
  int numClasses = (int)classLabels.size();
  
  for (auto p = priorClassFeatureCounts.begin(); p != priorClassFeatureCounts.end(); ++p) {
    Int_Double x = p->second;
    for (auto q = x.begin(); q != x.end(); ++q) classFeatureProbsSum[p->first] += q->second;
  }
  
  for (auto p = rowColValMap.begin(); p != rowColValMap.end(); ++p) {
    Int_Double colValMap = p->second;
    
    for (auto q = classLabels.begin(); q != classLabels.end(); ++q) {
      
      double classProb = (1+priorClassCounts[*q])/((double)numClasses + (double)numDocs);
      posteriorProbs[p->first][*q] += log(classProb);
      
      double constant = (double)classFeatureProbsSum[*q] + (double)numFeatures;
      
      double featureProb = 0;
      
      for (auto r = colValMap.begin(); r != colValMap.end(); ++r) {
        if (priorClassFeatureCounts[*q].find(r->first) != priorClassFeatureCounts[*q].end()) 
          featureProb += log(1+priorClassFeatureCounts[*q][r->first])-log(constant);
        
        else featureProb -= log(constant);
      }
      
      posteriorProbs[p->first][*q] += featureProb;
    }
    
    Int_Double x = posteriorProbs[p->first];
    
    double maxVal = -std::numeric_limits<double>::max();
    
    for (auto q = x.begin(); q != x.end(); ++q) if (q->second > maxVal) maxVal = q->second;
    
    for (auto q = x.begin(); q != x.end(); ++q) posteriorProbs[p->first][q->first] = exp((q->second)-maxVal);
  }
  
  return posteriorProbs;
}

Int_Int_Double computePosteriorProbabilities(Int_Int_Double &priorClassFeatureCounts, Int_Double &priorClassCounts, 
                                             Int_Int_Double &rowColValMap, const std::set<int> &classLabels,
                                             const int &numDocs, const int &numFeatures) {
  
  Int_Int_Double posteriorProbs = computePosteriors(priorClassFeatureCounts, priorClassCounts, 
                                                    rowColValMap, classLabels, numDocs, numFeatures);
  
  for (auto p = posteriorProbs.begin(); p != posteriorProbs.end(); ++p) {
    Int_Double x = p->second;

    double sum = 0;
    for (auto q = x.begin(); q != x.end(); ++q) sum += q->second;

    for (auto q = x.begin(); q != x.end(); ++q) posteriorProbs[p->first][q->first] = (q->second)/sum;
  }
  
  return posteriorProbs;
}

Int_Int_Double computePriorClassFeatureCounts(Int_Int_Double &rowClassProbs, Int_Int_Double &rowColValMap) {
  
  Int_Int_Double priorCounts;
  
  for (auto p = rowColValMap.begin(); p != rowColValMap.end(); ++p) {
    int row = p->first;
    
    Int_Double colValMap = p->second;
    Int_Double classProbs = rowClassProbs[row];
    
    for (auto q = classProbs.begin(); q != classProbs.end(); ++q) {
      for (auto r = colValMap.begin(); r != colValMap.end(); ++r) priorCounts[q->first][r->first] += (r->second)*(q->second);
    }
  }
  
  return priorCounts;
}

Int_Double computeClassCounts(Int_Int_Double &rowClassProbs) {
  Int_Double classPriorCounts;
  
  for (auto p = rowClassProbs.begin(); p != rowClassProbs.end(); ++p) {
    Int_Double classProbs = p->second;
    for (auto q = classProbs.begin(); q != classProbs.end(); ++q) classPriorCounts[q->first] += (q->second);
  }
  
  return classPriorCounts;
}

double likelihood(Int_Int_Double &priorClassFeatureCounts, Int_Double &priorClassCounts,
                  Int_Int_Double &rowColValMap, const std::set<int> &classLabels, 
                  const int &numDocs, const int &numFeatures) {
  
  Int_Int_Double posteriorProbs;
  
  Int_Double classFeatureProbsSum;
  
  int numClasses = (int)classLabels.size();
  
  for (auto p = priorClassFeatureCounts.begin(); p != priorClassFeatureCounts.end(); ++p) {
    Int_Double x = p->second;
    for (auto q = x.begin(); q != x.end(); ++q) classFeatureProbsSum[p->first] += q->second;
  }
  
  double likelh = 0;
  
  for (auto p = rowColValMap.begin(); p != rowColValMap.end(); ++p) {
    Int_Double colValMap = p->second;
    
    for (auto q = classLabels.begin(); q != classLabels.end(); ++q) {
      
      double classProb = (1+priorClassCounts[*q])/((double)numClasses + (double)numDocs);
      posteriorProbs[p->first][*q] += log(classProb);
      
      double constant = (double)classFeatureProbsSum[*q] + (double)numFeatures;
      
      double featureProb = 0;
      
      for (auto r = colValMap.begin(); r != colValMap.end(); ++r) {
        if (priorClassFeatureCounts[*q].find(r->first) != priorClassFeatureCounts[*q].end()) 
          featureProb += log(1+priorClassFeatureCounts[*q][r->first])-log(constant);
        
        else featureProb -= log(constant);
      }
      
      posteriorProbs[p->first][*q] += featureProb;
    }
    
    Int_Double x = posteriorProbs[p->first];
    
    double sum = 0;
    for (auto q = x.begin(); q != x.end(); ++q) sum += exp(q->second);
    
    likelh += log(sum);
  }
  
  return likelh;
}

// [[Rcpp::export]]
List cpp__nb(DataFrame inputSparseMatrix, std::vector<int> classLabels, 
             int numDocs, int numFeatures) {
  
  std::vector<int> rows = inputSparseMatrix["i"];
  std::vector<int> cols = inputSparseMatrix["j"];
  std::vector<double> vals = inputSparseMatrix["v"];
  
  std::set<int> uniqueRows(rows.begin(), rows.end());
  std::set<int> uniqueLabels(classLabels.begin(), classLabels.end());
  
  if (uniqueLabels.find(-1) != uniqueLabels.end()) uniqueLabels.erase(uniqueLabels.find(-1));
  
  Int_Int_Double rowColValMap, unlabelledDocsRowColValMap, labelledDocsRowColValMap;
  for (size_t i = 0; i < rows.size(); i++) rowColValMap[rows[i]][cols[i]] = vals[i];

  Int_Int rowClassMap;
  for (auto p = uniqueRows.begin(); p != uniqueRows.end(); ++p) rowClassMap[*p] = classLabels[*p-1];
  
  for (auto p = rowColValMap.begin(); p != rowColValMap.end(); ++p) {
    if (rowClassMap[p->first] == -1) unlabelledDocsRowColValMap[p->first] = p->second;
    else labelledDocsRowColValMap[p->first] = p->second;
  }
  

  Int_Int_Double rowClassProbs;

  for (auto p = uniqueRows.begin(); p != uniqueRows.end(); ++p) if (rowClassMap[*p] != -1) rowClassProbs[*p][rowClassMap[*p]] = 1;

  Int_Double priorClassCounts = computeClassCounts(rowClassProbs);
  Int_Int_Double priorClassFeatureCounts = computePriorClassFeatureCounts(rowClassProbs, rowColValMap);

  if (unlabelledDocsRowColValMap.size() > 0) {
    
    double lastLikelihood = likelihood(priorClassFeatureCounts, priorClassCounts, 
                                       rowColValMap, uniqueLabels, numDocs, numFeatures);
    
    while(true) {
      
      Int_Int_Double unlabelledDocsRowClassProbs = computePosteriorProbabilities(priorClassFeatureCounts, priorClassCounts, 
                                                                                 unlabelledDocsRowColValMap, uniqueLabels, numDocs, numFeatures);
      
      for (auto p = uniqueRows.begin(); p != uniqueRows.end(); ++p) if (rowClassMap[*p] == -1) rowClassProbs[*p] = unlabelledDocsRowClassProbs[*p];
      
      priorClassCounts = computeClassCounts(rowClassProbs);
      priorClassFeatureCounts = computePriorClassFeatureCounts(rowClassProbs, rowColValMap);
      
      double currLikelihood = likelihood(priorClassFeatureCounts, priorClassCounts, 
                                         rowColValMap, uniqueLabels, numDocs, numFeatures);
      
      if (std::abs(currLikelihood - lastLikelihood) < 0.1) break;
      
      std::cout << lastLikelihood << " " << currLikelihood << std::endl;
      
      lastLikelihood = currLikelihood;
    }
  }
  
  
  std::vector<int> clLabels1, clLabels2, featureLabels2;
  std::vector<double> clCounts1, clFeatureCounts2;
  
  for (auto p = priorClassCounts.begin(); p != priorClassCounts.end(); ++p) {
    clLabels1.push_back(p->first);
    clCounts1.push_back(p->second);
  }
  
  for (auto p = priorClassFeatureCounts.begin(); p != priorClassFeatureCounts.end(); ++p) {
    Int_Double x = p->second;
    for (auto q = x.begin(); q != x.end(); ++q) {
      clLabels2.push_back(p->first);
      featureLabels2.push_back(q->first);
      clFeatureCounts2.push_back(q->second);
    }
  }
  
  return List::create(_["ClassProbs"]=DataFrame::create(_["Class"]=clLabels1, _["Count"]=clCounts1),
                      _["ClassFeatureProbs"]=DataFrame::create(_["Class"]=clLabels2, 
                                              _["Feature"]=featureLabels2, 
                                              _["Count"]=clFeatureCounts2));
}

// [[Rcpp::export]]
Int_Int_Double cpp__nbTest(DataFrame inputSparseMatrix, List model, 
                           int numTrainDocs, int numTrainFeatures) {
  
  std::vector<int> rows = inputSparseMatrix["i"];
  std::vector<int> cols = inputSparseMatrix["j"];
  std::vector<double> vals = inputSparseMatrix["v"];
  
  DataFrame clCounts = as<DataFrame>(model["ClassProbs"]);
  DataFrame clFeatureCounts = as<DataFrame>(model["ClassFeatureProbs"]);
  
  Int_Int_Double rowColValMap;
  for (size_t i = 0; i < rows.size(); i++) rowColValMap[rows[i]][cols[i]] = vals[i];
  
  Int_Double priorClassCounts;
  Int_Int_Double priorClassFeatureCounts;
  
  std::vector<int> clLabels1 = as<std::vector<int>>(clCounts["Class"]);
  std::vector<double> clCounts1 = as<std::vector<double>>(clCounts["Count"]);
  
  std::set<int> uniqueLabels(clLabels1.begin(), clLabels1.end());
  
  std::vector<int> clLabels2 = as<std::vector<int>>(clFeatureCounts["Class"]);
  std::vector<int> featureLabels2 = as<std::vector<int>>(clFeatureCounts["Feature"]);
  std::vector<double> clFeatureCounts2 = as<std::vector<double>>(clFeatureCounts["Count"]);
  
  for (size_t i = 0; i < clLabels1.size(); i++) priorClassCounts[clLabels1[i]] = clCounts1[i];
  for (size_t i = 0; i < clLabels2.size(); i++) priorClassFeatureCounts[clLabels2[i]][featureLabels2[i]] = clFeatureCounts2[i];
  
  return computePosteriorProbabilities(priorClassFeatureCounts, priorClassCounts, 
                                       rowColValMap, uniqueLabels, numTrainDocs, numTrainFeatures);
}