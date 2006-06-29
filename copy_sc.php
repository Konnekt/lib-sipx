8<?
$verbose = 2;
include('adv_copy.php');

$srcDir = './';
$destDir = '__rozne/_sipX_noweTAPI_SC/';
$svnNotMask = 'D::.+/(\.svn|Debug|Release)/?';
$srcMask = '(D::)|(\.(cpp|c|hpp|h|wav|txt|vcproj|sln|rc|am|bat|xml)$)';
$srcRootMask = '^F::.+?/([^/.]+|[^/]+\.(sln|vcproj|txt|rc|h))$';
$copyFlag = ACOPY_MIRROR;
chdir(dirname(__FILE__));

//function advCopy($src , $dest , $mask=false , $notmask=false, $recursive=true, $applytodirs=false, $undo=false , $first=true) {
//function advCopy2($src , $dest , $mask=false , $notmask=false, $flags = ACOPY_RECURSIVE, $undo = false) {



// --------------------------

$src = $srcDir;
$dest = $destDir;

// --------------------------

/*
$src = $srcDir . "doc/";
$dest = $destDir . "doc/";

advCopy2("{$src}", "{$dest}", '^F::/.+\.(html|css)', '', $copyFlag);
*/

// --------------------------

$src = $srcDir . "sipXcallLib";
$dest = $destDir . "sipXcallLib";

advCopy2("{$src}", "{$dest}", $srcRootMask, '', $copyFlag);
advCopy2("{$src}/doc/doxyfile", "{$dest}/doc/doxyfile", false, $svnNotMask, $copyFlag);

advCopy2("{$src}/examples", "{$dest}/examples", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}/include", "{$dest}/include", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}/src", "{$dest}/src", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);


// --------------------------

$src = $srcDir . "sipXmediaAdapterLib";
$dest = $destDir . "sipXmediaAdapterLib";

advCopy2("{$src}", "{$dest}", $srcRootMask, '', $copyFlag);
advCopy2("{$src}/doc/doxyfile", "{$dest}/doc/doxyfile", false, $svnNotMask, $copyFlag);

advCopy2("{$src}/interface", "{$dest}/interface", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}/sipXmediaMediaProcessing", "{$dest}/sipXmediaMediaProcessing", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);


// --------------------------

$src = $srcDir . "sipXmediaLib/";
$dest = $destDir . "sipXmediaLib/";

advCopy2("{$src}", "{$dest}", $srcRootMask, '', $copyFlag);
advCopy2("{$src}doc/doxyfile", "{$dest}doc/doxyfile", false, $svnNotMask, $copyFlag);

advCopy2("{$src}include", "{$dest}include", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}src", "{$dest}src", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);


// --------------------------

$src = $srcDir . "sipXportLib/";
$dest = $destDir . "sipXportLib/";

advCopy2("{$src}", "{$dest}", $srcRootMask, '', $copyFlag);
advCopy2("{$src}doc/doxyfile", "{$dest}doc/doxyfile", false, $svnNotMask, $copyFlag);

advCopy2("{$src}include", "{$dest}include", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}src", "{$dest}src", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);


// --------------------------

$src = $srcDir . "sipXtackLib/";
$dest = $destDir . "sipXtackLib/";

advCopy2("{$src}", "{$dest}", $srcRootMask, '', $copyFlag);
advCopy2("{$src}doc/doxyfile", "{$dest}doc/doxyfile", false, $svnNotMask, $copyFlag);

advCopy2("{$src}examples", "{$dest}examples", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}include", "{$dest}include", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}src", "{$dest}src", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}siplog2siptrace", "{$dest}siplog2siptrace", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}siptest", "{$dest}siptest", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}sipviewer", "{$dest}sipviewer", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
advCopy2("{$src}syslog2siptrace", "{$dest}syslog2siptrace", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);


// --------------------------
/*
$src = $srcDir . "sipXtest/";
$dest = $destDir . "sipXtest/";

advCopy2("{$src}", "{$dest}", $srcMask, $svnNotMask, $copyFlag | ACOPY_RECURSIVE | ACOPY_APPLYTODIRS);
*/

?>
