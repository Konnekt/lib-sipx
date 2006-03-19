<?

require('adv_copy.php');

$verbose = 2;

$src = './';
$dest = '__copy/';

// function advCopy($src , $dest , $mask=false , $notmask=false, $recursive=true, $applytodirs=false, $undo=false , $first=true) {


advCopy($src, $dest, 'sipx.sln', false, false);

function copyLib($name) {
	 global $src, $dest;
	 $lsrc = $src . $name .'/';
	 $ldest = $dest . $name .'/';
	 advCopy($lsrc, $ldest, '\\.(vcproj|sln)$', false, false);
	 advCopy($lsrc . 'src/', $ldest . 'src/', false, '/\.svn(/.+)?$', true, true);
	 advCopy($lsrc . 'include/', $ldest . 'include/', false, '/\.svn(/.+)?$', true, true);
}

//advCopy($src, $dest, false, '/\.svn(/.+)?$', true, true);

copyLib('sipxcalllib');
copyLib('sipxmedialib');
copyLib('sipxtacklib');
copyLib('sipxportlib');
?>
