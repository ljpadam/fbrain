/*
 * Copyright or � or Copr. Universit� de Strasbourg - Centre National de la Recherche Scientifique
 *
 * 24 januar 2011
 * < pontabry at unistra dot fr >
 *
 * This software is governed by the CeCILL-B license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL-B
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL-B license and that you accept its terms.
 */

// TCLAP : Templatized C++ Command Line Parser includes
#include "CmdLine.h"

// STL includes
#include "string"
#include "iostream"

// VTK includes
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkAppendPolyData.h"
#include "vtkPolyDataWriter.h"

// Local includes
#include "btkTypes.h"
#include "btkPoint.h"
#include "btkDirection.h"
#include "btkSignal.h"
#include "btkSignalExtractor.h"
#include "btkSHModel.h"
#include "btkSHModelEstimator.h"
#include "btkSHModelDensity.h"
#include "btkNormalDensity.h"
#include "btkVonMisesFisherDensity.h"
#include "btkImportanceDensity.h"
#include "btkInitialDensity.h"
#include "btkAPrioriDensity.h"
#include "btkLikelihoodDensity.h"
#include "btkParticleFilter.h"


using namespace btk;


int main(int argc, char *argv[])
{
    // Command line variables
    std::string dwiFileName;
    std::string vecFileName;
    std::string maskFileName;
    std::string labelFilename;

    unsigned int modelOrder;
    Real lambda;
    unsigned int nbOfParticles;
    Real stepSize;
    Real epsilon;
    Real Kappa;
    Real angleThreshold;


    try
    {
        //
        // Program's command line parser definition
        //

            // Defines command line parser
            TCLAP::CmdLine cmd("BTK Tractography", ' ', "0.2");

            // Defines arguments
            TCLAP::ValueArg<std::string>   dwiArg("d", "dwi", "Dwi sequence", true, "", "std::string", cmd);
            TCLAP::ValueArg<std::string>   vecArg("v", "vectors", "Gradient vectors", true, "", "std::string", cmd);
            TCLAP::ValueArg<std::string>  maskArg("m", "mask", "White matter mask", true, "", "std::string", cmd);
            TCLAP::ValueArg<std::string> labelArg("l", "label", "Label volume of seeds", true, "", "std::string", cmd);

            TCLAP::ValueArg<unsigned int> orderArg("", "model-order", "Order of the model (i.e. of spherical harmonics)", false, 4, "unsigned int", cmd);
            TCLAP::ValueArg<Real>    lambdArg("", "model-regularization", "Regularization coefficient of the model", false, 0.006, "Real", cmd);
            TCLAP::ValueArg<unsigned int> particlesArg("", "number-of-particles", "Number of particles", false, 1000, "unsigned int", cmd);
            TCLAP::ValueArg<Real>    epsilonArg("", "resampling-threshold", "Resampling treshold", false, 0.6, "Real", cmd);
            TCLAP::ValueArg<Real>    stepSizeArg("", "step-size", "Step size of particles displacement", false, 0.5, "Real", cmd);
            TCLAP::ValueArg<Real>    KappaArg("", "curve-constraint", "Curve constraint of a particle's trajectory", false, 30.0, "Real", cmd);
            TCLAP::ValueArg<Real>    angleThreshArg("", "angular-threshold", "Angular threshold between successive displacement vector of a particle's trajectory", false, M_PI/3., "Real", cmd);

            // Parsing arguments
            cmd.parse(argc, argv);

            // Get back arguments' values
            dwiFileName    = dwiArg.getValue();
            vecFileName    = vecArg.getValue();
            maskFileName   = maskArg.getValue();
            labelFilename  = labelArg.getValue();

            modelOrder     = orderArg.getValue();
            lambda         = lambdArg.getValue();
            nbOfParticles  = particlesArg.getValue();
            epsilon        = epsilonArg.getValue();
            stepSize       = stepSizeArg.getValue();
            Kappa          = KappaArg.getValue();
            angleThreshold = angleThreshArg.getValue();
    }
    catch(TCLAP::ArgException &e)
    {
        std::cout << "TCLAP error: " << e.error() << " for argument " << e.argId() << std::endl;
        std::exit(EXIT_FAILURE);
    }


    //
    // Diffusion signal extraction
    //

        SignalExtractor *extractor = new SignalExtractor(vecFileName, dwiFileName, maskFileName);
        extractor->extract();

        std::vector<Direction> *directions = extractor->GetDirections();
        Sequence::Pointer       signal     = extractor->GetSignal();
        std::vector<Real>      *sigmas     = extractor->GetSigmas();
        Mask::Pointer           mask       = extractor->GetMask();

        delete extractor;



    //
    // Model estimation
    //

        SHModelEstimator *estimator = new SHModelEstimator(signal, directions, mask, modelOrder, lambda);
        estimator->estimate();

        Sequence::Pointer model = estimator->GetModel();

        delete estimator;



    //
    // Tractography (using particles filter)
    //


        // Get spherical harmonics model
        SHModel *modelFun = new SHModel(model);

        // Get signal
        Signal *signalFun = new Signal(signal, sigmas, directions);

        // Read label image if any and verify sizes
        ImageReader::Pointer labelReader = ImageReader::New();
        labelReader->SetFileName(labelFilename);
        labelReader->Update();
        Image::Pointer labelVolume = labelReader->GetOutput();

        // These densities will be used next
        SHModelDensity modelDensity(modelFun);
        NormalDensity normalDensity;
        VonMisesFisherDensity vmf(Kappa);


//        vtkSmartPointer<vtkPolyData>       fibers = vtkSmartPointer<vtkPolyData>::New();
        vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
//        append->UserManagedInputsOn();

        Image::Pointer connectMap = Image::New();
        connectMap->SetOrigin(signalFun->getOrigin());
        connectMap->SetSpacing(signalFun->getSpacing());
        connectMap->SetRegions(signalFun->getSize());
        connectMap->SetDirection(modelFun->GetDirection());
        connectMap->Allocate();
        connectMap->FillBuffer(0);


        // Apply filter on each labeled voxels
        ImageIterator it(labelVolume, labelVolume->GetLargestPossibleRegion());

        for(it.GoToBegin(); !it.IsAtEnd(); ++it)
        {
            int label = (int)it.Get();

            if(label != 0)
            {
                Image::IndexType index = it.GetIndex();
                itk::Point<Real,3> worldPoint;
                labelVolume->TransformIndexToPhysicalPoint(index,worldPoint);
                Point begin(worldPoint[0], worldPoint[1], worldPoint[2]);

                // Set up filter's densities
                std::cout << "Setting up needed densities for label " << label << begin << " ..." << std::endl;
                ImportanceDensity importance(vmf, modelFun, angleThreshold);
                InitialDensity    initial(modelDensity, begin);
                APrioriDensity    apriori(vmf);
                LikelihoodDensity likelihood(normalDensity, signalFun, modelFun);
                std::cout << "done." << std::endl;


                // Let's start filtering
                std::cout << "Filtering label " << label << "..." << std::endl;

                ParticleFilter filter(modelFun, initial, apriori, likelihood, importance, mask, signalFun->getSize(), signalFun->getOrigin(), signalFun->getSpacing(), nbOfParticles, begin, epsilon, stepSize);
                filter.run(label);

//                append->SetInputByNumber(0, fibers);
//                append->SetInputByNumber(1, filter.GetFiber());
//                append->Update();
//                fibers = append->GetOutput();
                append->AddInput(filter.GetFiber());

                Image::Pointer map = filter.GetConnectionMap();
                ImageIterator out(connectMap, connectMap->GetLargestPossibleRegion());
                ImageIterator  in(map, map->GetLargestPossibleRegion());

                for(in.GoToBegin(), out.GoToBegin(); !in.IsAtEnd() && !out.IsAtEnd(); ++in, ++out)
                    out.Set(out.Get() + in.Get());

                std::cout << "done." << std::endl;
            }
        } // for each labeled voxels

        delete signalFun;
        delete modelFun;
        delete directions;
        delete sigmas;


    //
    // Writing files
    //

        // Normalize connection map
        Real max = 0;
        it = ImageIterator(connectMap, connectMap->GetLargestPossibleRegion());

        for(it.GoToBegin(); !it.IsAtEnd(); ++it)
        {
            if(max < it.Get())
                max = it.Get();
        }

        for(it.GoToBegin(); !it.IsAtEnd(); ++it)
            it.Set(it.Get() / max);


        // Write connection map image
        try
        {
            ImageWriter::Pointer writer = ImageWriter::New();
            writer->SetFileName("map.nii.gz");
            writer->SetInput(connectMap);
            writer->Update();
        }
        catch(itk::ImageFileWriterException &err)
        {
            std::cout << "Error: " << std::endl;
            std::cout << err << std::endl;
        }

        // Write fiber polydata
        append->Update();
        vtkSmartPointer<vtkPolyDataWriter> writer = vtkSmartPointer<vtkPolyDataWriter>::New();
        writer->SetInput(append->GetOutput());
        writer->SetFileName("fibers.vtk");
        writer->SetFileTypeToBinary();
        writer->Write();


    return EXIT_SUCCESS;
}