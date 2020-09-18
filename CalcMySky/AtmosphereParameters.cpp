#include "AtmosphereParameters.hpp"
#include <optional>
#include "Spectrum.hpp"
#include "../common/const.hpp"

namespace
{

unsigned long long getUInt(QString const& value, const unsigned long long min, const unsigned long long max,
                           QString const& filename, int lineNumber)
{
    bool ok;
    const auto x=value.toULongLong(&ok);
    if(!ok)
    {
        std::cerr << filename << ":" << lineNumber << ": can't parse integer\n";
    }
    if(x<min || x>max)
    {
        std::cerr << filename << ":" << lineNumber << ": integer out of range. Valid range is [" << min << ".." << max << "]\n";
        throw MustQuit{};
    }
    return x;
}

struct Quantity
{
    virtual std::string name() const = 0;
    virtual std::map<QString, double> units() const = 0;
    virtual QString basicUnit() const = 0;
};

struct LengthQuantity : Quantity
{
    std::string name() const override { return "length"; }
    std::map<QString, double> units() const override
    {
        return {
                {"nm",1e-9},
                {"um",1e-6},
                {"mm",1e-3},
                { "m",1e+0},
                {"km",1e+3},
                {"Mm",1e+6},
                {"Gm",1e+9},
                {"AU",astronomicalUnit},
               };
    }
    QString basicUnit() const override { return "m"; }
};

struct WavelengthQuantity : LengthQuantity
{
    QString basicUnit() const override { return "nm"; }
};

struct ReciprocalLengthQuantity : Quantity
{
    std::string name() const override { return "reciprocal length"; }
    std::map<QString, double> units() const override
    {
        return {
                {"nm^-1",1e+9},
                {"um^-1",1e+6},
                {"mm^-1",1e+3},
                { "m^-1",1e-0},
                {"km^-1",1e-3},
                {"Mm^-1",1e-6},
                {"Gm^-1",1e-9},
               };
    }
    QString basicUnit() const override { return "m^-1"; }
};

struct AreaQuantity : Quantity
{
    std::string name() const override { return "area"; }
    std::map<QString, double> units() const override
    {
        return {
                {"am^2",1e-36},
                {"fm^2",1e-30},
                {"pm^2",1e-24},
                {"nm^2",1e-18},
                {"um^2",1e-12},
                {"mm^2",1e-6},
                {"cm^2",1e-4},
                { "m^2",1e-0},
                {"km^2",1e+6},
                {"Mm^2",1e+12},
                {"Gm^2",1e+18},
               };
    }
    QString basicUnit() const override { return "m^2"; }
};

struct DimensionlessQuantity {};

double getQuantity(QString const& value, const double min, const double max, DimensionlessQuantity const&,
                   QString const& filename, int lineNumber)
{
    bool ok;
    const auto x=value.toDouble(&ok);
    if(!ok)
    {
        std::cerr << filename << ":" << lineNumber << ": failed to parse number\n";
        throw MustQuit{};
    }
    if(x<min || x>max)
    {
        std::cerr << filename << ":" << lineNumber << ": value " << x << " is out of range. Valid range is [" << min << ".." << max << "].\n";
        throw MustQuit{};
    }
    return x;
}

double getQuantity(QString const& value, const double min, const double max, Quantity const& quantity, QString const& filename, const int lineNumber, QString const& errorMessagePrefix="")
{
    auto regex=QRegExp("(-?[0-9.]+) *([a-zA-Z][a-zA-Z^-0-9]*)");
    if(!regex.exactMatch(value))
    {
        std::cerr << filename << ":" << lineNumber << ": " << errorMessagePrefix << "bad format of " << quantity.name()
                  << " quantity. Must be `NUMBER UNIT', e.g. `30.2 " << quantity.basicUnit() << "' (without the quotes).\n";
        throw MustQuit{};
    }
    bool ok;
    const auto x=regex.cap(1).toDouble(&ok);
    if(!ok)
    {
        std::cerr << filename << ":" << lineNumber << ": failed to parse numeric part of the quantity\n";
        throw MustQuit{};
    }
    const auto units=quantity.units();
    const auto unit=regex.cap(2).trimmed();
    const auto scaleIt=units.find(unit);
    if(scaleIt==units.end())
    {
        std::cerr << filename << ":" << lineNumber << ": unrecognized " << quantity.name() << " unit " << unit << ". Can be one of ";
        for(auto it=units.begin(); it!=units.end(); ++it)
        {
            if(it!=units.begin()) std::cerr << ',';
            std::cerr << it->first;
        }
        std::cerr << ".\n";
        throw MustQuit{};
    }
    const auto finalX = x * scaleIt->second;
    if(finalX<min || finalX>max)
    {
        std::cerr << filename << ":" << lineNumber << ": value " << finalX << " " << quantity.basicUnit()
                  << " is out of range. Valid range is [" << min << ".." << max << "] " << quantity.basicUnit() << ".\n";
        throw MustQuit{};
    }
    return finalX;
}

QString readGLSLFunctionBody(QTextStream& stream, const QString filename, int& lineNumber)
{
    QString function;
    const QRegExp startEndMarker("^\\s*```\\s*$");
    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        if(!begun)
        {
            if(!startEndMarker.exactMatch(line))
            {
                std::cerr << filename << ":" << lineNumber << ": function body must start and end with triple backtick placed on a separate line.\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }

        if(!startEndMarker.exactMatch(line))
            function.append(line+'\n');
        else
            break;
    }
    return function;
}

std::vector<glm::vec4> getSpectrum(std::vector<glm::vec4> const& allWavelengths, QString const& line, const GLfloat min, const GLfloat max,
                                 QString const& filename, const int lineNumber, const bool checkSize=true)
{
    if(line.startsWith("file "))
    {
        if(allWavelengths.empty())
        {
            std::cerr << filename << ":" << lineNumber << ": error: tried to read a spectrum file without having read list of wavelengths\n";
            throw MustQuit{};
        }
        auto path=line.mid(5);
        const QFileInfo fi(path);
        if(!fi.isAbsolute())
            path=QFileInfo(filename).absolutePath()+"/"+path;
        QFile file(path);
        if(!file.open(QFile::ReadOnly))
        {
            std::cerr << filename << ":" << lineNumber << ": failed to open the file " << path << ": " << file.errorString() << "\n";
            throw MustQuit{};
        }
        const auto spectrum=Spectrum::parseFromCSV(file.readAll(),path,1)
                                            .resample(allWavelengths.front()[0],
                                                      allWavelengths.back()[AtmosphereParameters::pointsPerWavelengthItem-1],
                                                      allWavelengths.size()*AtmosphereParameters::pointsPerWavelengthItem);
        const auto& values=spectrum.values;
        std::vector<glm::vec4> output;
        for(unsigned i=0; i<values.size(); i+=4)
            output.emplace_back(values[i+0], values[i+1], values[i+2], values[i+3]);
        return output;
    }
    const auto items=line.split(',');
    if(checkSize && size_t(items.size()) != allWavelengths.size()*AtmosphereParameters::pointsPerWavelengthItem)
    {
            std::cerr << filename << ":" << lineNumber << ": spectrum has " << items.size() << " entries, but there are "
                      << allWavelengths.size()*AtmosphereParameters::pointsPerWavelengthItem << " wavelengths\n";
            throw MustQuit{};
    }
    if(items.size()%4)
    {
            std::cerr << filename << ":" << lineNumber << ": spectrum length must be a multiple of 4\n";
            throw MustQuit{};
    }
    std::vector<GLfloat> values;
    for(int i=0; i<items.size(); ++i)
    {
        bool ok;
        const auto value=items[i].toFloat(&ok);
        if(!ok)
        {
            std::cerr << filename << ":" << lineNumber << ": failed to parse entry #" << i+1 << "\n";
            throw MustQuit{};
        }
        if(value<min)
        {
            std::cerr << filename << ":" << lineNumber << ": spectrum point #" << i+1 << " is less than minimally allowed: " << value << " < " << min << "\n";
            throw MustQuit{};
        }
        if(value>max)
        {
            std::cerr << filename << ":" << lineNumber << ": spectrum point #" << i+1 << " is greater than maximally allowed: " << value << " > " << max << "\n";
            throw MustQuit{};
        }
        values.emplace_back(value);
    }
    std::vector<glm::vec4> spectrum;
    for(unsigned i=0; i<values.size(); i+=4)
        spectrum.emplace_back(values[i+0], values[i+1], values[i+2], values[i+3]);
    return spectrum;
}

std::vector<glm::vec4> getWavelengthRange(QString const& line, const GLfloat minWL_nm, const GLfloat maxWL_nm,
                                          QString const& filename, const int lineNumber)
{
    constexpr GLfloat nm=1e-9f;
    const auto items=line.split(',');
    std::optional<GLfloat> minOpt;
    std::optional<GLfloat> maxOpt;
    std::optional<int> countOpt;
    for(const auto& item : items)
    {
        if(QRegExp minRX("\\s*min\\s*=\\s*(.+)\\s*"); minRX.exactMatch(item))
        {
            if(minOpt)
            {
                std::cerr << filename << ":" << lineNumber << ": bad wavelength range: extra `min' key\n";
                throw MustQuit{};
            }
            minOpt=getQuantity(minRX.capturedTexts()[1], minWL_nm*nm, maxWL_nm*nm, WavelengthQuantity{},
                               filename, lineNumber, "wavelength range minimum: ");
        }
        else if(QRegExp maxRX("\\s*max\\s*=\\s*(.+)\\s*"); maxRX.exactMatch(item))
        {
            if(maxOpt)
            {
                std::cerr << filename << ":" << lineNumber << ": bad wavelength range: extra `max' key\n";
                throw MustQuit{};
            }
            maxOpt=getQuantity(maxRX.capturedTexts()[1], minWL_nm*nm, maxWL_nm*nm, WavelengthQuantity{},
                               filename, lineNumber, "wavelength range maximum: ");
        }
        else if(QRegExp countRX("\\s*count\\s*=\\s*([0-9]+)\\s*"); countRX.exactMatch(item))
        {
            if(countOpt)
            {
                std::cerr << filename << ":" << lineNumber << ": bad wavelength range: extra `count' key\n";
                throw MustQuit{};
            }
            bool ok=false;
            countOpt=countRX.capturedTexts()[1].toInt(&ok);
            if(!ok)
            {
                std::cerr << filename << ":" << lineNumber << ": wavelength range: failed to parse range count.\n";
                throw MustQuit{};
            }
        }
    }
    if(!minOpt)
    {
        std::cerr << filename << ":" << lineNumber << ": invalid range: missing `min' key\n";
        throw MustQuit{};
    }
    if(!maxOpt)
    {
        std::cerr << filename << ":" << lineNumber << ": invalid range: missing `max' key\n";
        throw MustQuit{};
    }
    if(!countOpt)
    {
        std::cerr << filename << ":" << lineNumber << ": invalid range: missing `count' key\n";
        throw MustQuit{};
    }
    const auto min=*minOpt/nm, max=*maxOpt/nm;
    const int count=*countOpt;
    if(count<=0 || count%4)
    {
        std::cerr << filename << ":" << lineNumber << ": range element count must be a positive multple of 4.\n";
        throw MustQuit{};
    }
    std::vector<glm::vec4> values;
    const auto range=max-min;
    for(int i=0;i<count;i+=4)
    {
        values.push_back(glm::vec4(min+range*double(i+0)/(count-1),
                                   min+range*double(i+1)/(count-1),
                                   min+range*double(i+2)/(count-1),
                                   min+range*double(i+3)/(count-1)));
    }
    return values;
}

AtmosphereParameters::Scatterer parseScatterer(QTextStream& stream, QString const& name, QString const& filename, int& lineNumber)
{
    AtmosphereParameters::Scatterer description(name);
    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');

        if(!begun)
        {
            if(keyValue.size()!=1 || keyValue[0] != "{")
            {
                std::cerr << filename << ":" << lineNumber << ": scatterer description must begin with a '{'\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }
        if(keyValue.size()==1 && keyValue[0]=="}")
            break;

        if(keyValue.size()!=2)
        {
            std::cerr << filename << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();

        if(key=="cross section at 1 um")
            description.crossSectionAt1um=getQuantity(value,1e-35,1,AreaQuantity{},filename,lineNumber);
        else if(key=="angstrom exponent")
            description.angstromExponent=getQuantity(value,-10,10,DimensionlessQuantity{},filename,lineNumber);
        else if(key=="number density")
            description.numberDensity=readGLSLFunctionBody(stream,filename,++lineNumber);
        else if(key=="phase function")
            description.phaseFunction=readGLSLFunctionBody(stream,filename,++lineNumber);
        else if(key=="phase function type")
            description.phaseFunctionType=parsePhaseFunctionType(value,filename,lineNumber);
    }
    if(!description.valid())
    {
        std::cerr << "Description of scatterer \"" << name << "\" is incomplete\n";
        throw MustQuit{};
    }

    return description;
}

AtmosphereParameters::Absorber parseAbsorber(AtmosphereParameters const& atmo, QTextStream& stream, QString const& name, QString const& filename, int& lineNumber)
{
    AtmosphereParameters::Absorber description(name, atmo);

    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');

        if(!begun)
        {
            if(keyValue.size()!=1 || keyValue[0] != "{")
            {
                std::cerr << filename << ":" << lineNumber << ": absorber description must begin with a '{'\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }
        if(keyValue.size()==1 && keyValue[0]=="}")
            break;

        if(keyValue.size()!=2)
        {
            std::cerr << filename << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();

        if(key=="number density")
            description.numberDensity=readGLSLFunctionBody(stream,filename,++lineNumber);
        else if(key=="cross section")
            description.absorptionCrossSection=getSpectrum(atmo.allWavelengths,value,0,10,filename,lineNumber);
    }
    if(!description.valid())
    {
        std::cerr << "Description of absorber \"" << name << "\" is incomplete\n";
        throw MustQuit{};
    }

    return description;
}

}

void AtmosphereParameters::parse(QString const& atmoDescrFileName)
{
    QFile atmoDescr(atmoDescrFileName);
    if(!atmoDescr.open(QIODevice::ReadOnly))
    {
        std::cerr << "Failed to open atmosphere description file: " << atmoDescr.errorString() << '\n';
        throw MustQuit{};
    }
    QTextStream stream(&atmoDescr);
    int lineNumber=1;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        QRegExp scattererDescriptionKey("^scatterer \"([^\"]+)\"$");
        QRegExp absorberDescriptionKey("^absorber \"([^\"]+)\"$");
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');
        if(keyValue.size()!=2)
        {
            std::cerr << atmoDescrFileName << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }

        constexpr auto GLSIZEI_MAX = std::numeric_limits<GLsizei>::max();
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();
        if(key=="transmittance texture size for vza")
            transmittanceTexW=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="transmittance texture size for altitude")
            transmittanceTexH=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="transmittance integration points")
            numTransmittanceIntegrationPoints=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="radial integration points")
            radialIntegrationPoints=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="angular integration points per half revolution")
            angularIntegrationPointsPerHalfRevolution=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture size for sza")
            irradianceTexW=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture size for altitude")
            irradianceTexH=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for vza")
        {
            const auto integer=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
            if(integer%2)
            {
                std::cerr << atmoDescrFileName << ":" << lineNumber << ": value for \"" << key << "\" must be even (shaders rely on this)\n";
                throw MustQuit{};
            }
            scatteringTextureSize[0]=integer;
        }
        else if(key=="scattering texture size for dot(view,sun)")
            scatteringTextureSize[1]=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for sza")
            scatteringTextureSize[2]=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for altitude")
            scatteringTextureSize[3]=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="eclipsed scattering texture size for relative azimuth")
            eclipsedSingleScatteringTextureSize[0]=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="eclipsed scattering texture size for vza")
            eclipsedSingleScatteringTextureSize[1]=getUInt(value,1,GLSIZEI_MAX, atmoDescrFileName, lineNumber);
        else if(key=="earth radius")
            earthRadius=getQuantity(value,1,1e10,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="atmosphere height")
            atmosphereHeight=getQuantity(value,1,1e6,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="earth-sun distance")
        {
            earthSunDistance=getQuantity(value,0.5*AU,1e20*AU,LengthQuantity{},atmoDescrFileName,lineNumber);
            sunAngularRadius=sunRadius/earthSunDistance;
        }
        else if(key=="earth-moon distance")
        {
            earthMoonDistance=getQuantity(value,1e-4*AU,1e20*AU,LengthQuantity{},atmoDescrFileName,lineNumber);
            // moonAngularRadius is computed from earthMoonDistance and other parameters on the fly
        }
        else if(key=="wavelengths")
            allWavelengths=getWavelengthRange(value,100,100'000,atmoDescrFileName,lineNumber);
        else if(key=="solar irradiance at toa")
            solarIrradianceAtTOA=getSpectrum(allWavelengths,value,0,1e3,atmoDescrFileName,lineNumber);
        else if(key.contains(scattererDescriptionKey))
        {
            const auto& name=scattererDescriptionKey.cap(1);
            if(std::find_if(scatterers.begin(), scatterers.end(),
                            [&](const auto& existing) { return existing.name==name; }) != scatterers.end())
            {
                std::cerr << atmoDescrFileName << ":" << lineNumber << ": duplicate scatterer \"" << name << "\"\n";
                throw MustQuit{};
            }
            scatterers.emplace_back(parseScatterer(stream, name, atmoDescrFileName,++lineNumber));
        }
        else if(key.contains(absorberDescriptionKey))
            absorbers.emplace_back(parseAbsorber(*this, stream, absorberDescriptionKey.cap(1), atmoDescrFileName,++lineNumber));
        else if(key=="scattering orders")
            scatteringOrdersToCompute=getQuantity(value,1,100, DimensionlessQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="ground albedo")
        {
            groundAlbedo=getSpectrum(allWavelengths, value, 0, 1, atmoDescrFileName, lineNumber);
        }
        else
            std::cerr << "WARNING: Unknown key: " << key << "\n";
    }
    if(!stream.atEnd())
    {
        std::cerr << atmoDescrFileName << ":" << lineNumber << ": error: failed to read file\n";
        throw MustQuit{};
    }
    if(allWavelengths.empty())
    {
        std::cerr << "Wavelengths aren't specified in atmosphere description\n";
        throw MustQuit{};
    }
    if(solarIrradianceAtTOA.empty())
    {
        std::cerr << "Solar irradiance at TOA isn't specified in atmosphere description\n";
        throw MustQuit{};
    }
    if(groundAlbedo.empty())
    {
        std::cerr << "Warning: ground albedo was not specified, assuming 100% white.\n";
        groundAlbedo=std::vector<glm::vec4>(allWavelengths.size(), glm::vec4(1));
    }
}